/* Maple Bus host implementation.
 * Ported from DreamPicoPort (OrangeFox86) via OGX-Mini-2026. MIT licensed.
 *
 * Both PIO programs are 29 instructions and a PIO block holds 32, so unlike
 * the upstreams (which use one block per direction) this keeps a single block
 * and swaps the programs during the write-to-read handoff. DreamPicoPort
 * measured that a peripheral starts responding somewhere after 50 us from the
 * bus going neutral, and that its own handoff completes within 1 us; the extra
 * instruction-memory rewrite is 29 word stores on top of that.
 */

#include "MapleBus.h"
#include "maple_config.h"
#include "maple_utils.h"
#include <pico/stdlib.h>
#include <hardware/dma.h>
#include <hardware/irq.h>
#include <hardware/gpio.h>
#include <cstring>

#define MAPLE_PROG_LEN (sizeof(maple_out_program_instructions) / sizeof(maple_out_program_instructions[0]))

/* Instruction words are staged in RAM so the handoff never waits on XIP. */
static uint16_t s_instrOut[MAPLE_PROG_LEN];
static uint16_t s_instrIn[MAPLE_PROG_LEN];

static MapleBus* s_bus = nullptr;

/* The handlers themselves must be in RAM too, not just what they call: the
 * vector table points straight at these, so leaving them in flash would put a
 * XIP fetch on the front of the write-to-read handoff. */
static void __not_in_flash_func(maple_write_irq_handler)(void) {
    if (s_bus) s_bus->writeIsr();
}
static void __not_in_flash_func(maple_read_irq_handler)(void) {
    if (s_bus) s_bus->readIsr();
}

MapleBus::MapleBus(uint8_t pio, uint32_t pinA, int32_t dirPin, bool dirOutHigh)
    : mPio(pio ? pio1 : pio0)
    , mPinA(pinA)
    , mPinB(pinA + 1)
    , mDirPin(dirPin)
    , mDirOutHigh(dirOutHigh)
    , mMaskA(1u << pinA)
    , mMaskB(1u << (pinA + 1))
    , mMaskAB(mMaskA | mMaskB)
    , mSmOut(pio_claim_unused_sm(mPio, true))
    , mSmIn(pio_claim_unused_sm(mPio, true))
    , mDmaWriteChannel(dma_claim_unused_channel(true))
    , mDmaReadChannel(dma_claim_unused_channel(true))
    , mLoaded(Loaded::NONE)
    , mCurrentPhase(Phase::IDLE)
    , mExpectingResponse(false)
    , mResponseTimeoutUs(MAPLE_RESPONSE_TIMEOUT_US)
    , mRxByteOrder(MaplePacket::ByteOrder::HOST)
    , mProcKillTime(UINT64_MAX)
    , mLastReceivedWordTimeUs(0)
    , mLastReadTransferCount(0)
{
    memcpy(s_instrOut, maple_out_program_instructions, sizeof(s_instrOut));
    memcpy(s_instrIn, maple_in_program_instructions, sizeof(s_instrIn));

    /* Reserve instruction memory 0..28 once so nothing else claims it. Only
     * one of the two programs is resident at any moment. */
    pio_add_program_at_offset(mPio, &maple_out_program, MAPLE_PROGRAM_OFFSET);
    mLoaded = Loaded::OUT;

    /* Out SM: clock derived from MAPLE_NS_PER_BIT and the phase ticks that
     * maple_out was written around. */
    mCfgOut = maple_out_program_get_default_config(MAPLE_PROGRAM_OFFSET);
    sm_config_set_sideset_pins(&mCfgOut, mPinA);
    sm_config_set_set_pins(&mCfgOut, mPinA, 2);
    sm_config_set_out_shift(&mCfgOut, false, true, 32);
    float div = (MAPLE_CPU_FREQ_KHZ * (MAPLE_NS_PER_BIT / 3.0f * 2.0f)) / maple_out_DOUBLE_PHASE_TICKS / 1000000.0f;
    sm_config_set_clkdiv(&mCfgOut, div);

    /* In SM: samples as fast as possible. */
    mCfgIn = maple_in_program_get_default_config(MAPLE_PROGRAM_OFFSET);
    sm_config_set_in_pins(&mCfgIn, mPinA);
    sm_config_set_jmp_pin(&mCfgIn, mPinA);
    sm_config_set_out_shift(&mCfgIn, true, false, 32);
    sm_config_set_in_shift(&mCfgIn, false, true, 32);
    sm_config_set_clkdiv(&mCfgIn, 1.0f);

    pio_sm_init(mPio, mSmOut, MAPLE_PROGRAM_OFFSET, &mCfgOut);

    gpio_set_dir_in_masked(mMaskAB);
    gpio_set_pulls(mPinA, true, false);
    gpio_set_pulls(mPinB, true, false);

    if (mDirPin >= 0) {
        gpio_init((uint)mDirPin);
        setDirection(false);
        gpio_set_dir((uint)mDirPin, true);
    }

    /* Both state machines live on the same block now, so they raise different
     * PIO interrupt flags: "irq ... rel" makes the flag index equal to the
     * state machine index. Route each to its own NVIC line. */
    s_bus = this;
    const uint irqOut = (mPio == pio0) ? PIO0_IRQ_0 : PIO1_IRQ_0;
    const uint irqIn  = (mPio == pio0) ? PIO0_IRQ_1 : PIO1_IRQ_1;

    pio_set_irq0_source_enabled(mPio, (pio_interrupt_source_t)(pis_interrupt0 + mSmOut), true);
    irq_set_exclusive_handler(irqOut, maple_write_irq_handler);
    irq_set_enabled(irqOut, true);

    pio_set_irq1_source_enabled(mPio, (pio_interrupt_source_t)(pis_interrupt0 + mSmIn), true);
    irq_set_exclusive_handler(irqIn, maple_read_irq_handler);
    irq_set_enabled(irqIn, true);

    /* DMA write: memory -> PIO TX FIFO, byte swapped. */
    dma_channel_config dc = dma_channel_get_default_config(mDmaWriteChannel);
    channel_config_set_read_increment(&dc, true);
    channel_config_set_write_increment(&dc, false);
    channel_config_set_bswap(&dc, true);
    channel_config_set_dreq(&dc, pio_get_dreq(mPio, mSmOut, true));
    dma_channel_configure(mDmaWriteChannel, &dc,
        &mPio->txf[mSmOut], mWriteBuffer, 1, false);

    /* DMA read: PIO RX FIFO -> memory, byte swapped. */
    dma_channel_config dr = dma_channel_get_default_config(mDmaReadChannel);
    channel_config_set_read_increment(&dr, false);
    channel_config_set_write_increment(&dr, true);
    channel_config_set_bswap(&dr, true);
    channel_config_set_dreq(&dr, pio_get_dreq(mPio, mSmIn, false));
    dma_channel_configure(mDmaReadChannel, &dr,
        mReadBuffer, &mPio->rxf[mSmIn], sizeof(mReadBuffer) / sizeof(mReadBuffer[0]), false);
}

void __not_in_flash_func(MapleBus::loadProgram)(Loaded which) {
    if (mLoaded == which) return;
    const uint16_t* src = (which == Loaded::OUT) ? s_instrOut : s_instrIn;
    for (uint i = 0; i < MAPLE_PROG_LEN; i++) {
        mPio->instr_mem[i] = src[i];
    }
    mLoaded = which;
}

static void crc8_word(uint32_t word, uint8_t& crc) {
    const uint8_t* p = (const uint8_t*)&word;
    for (unsigned i = 0; i < 4; i++) crc ^= p[i];
}
static void crc8_words(const uint32_t* src, uint32_t len, uint8_t& crc) {
    for (; len; len--, src++) crc8_word(*src, crc);
}

bool MapleBus::lineCheck() {
#if (MAPLE_OPEN_LINE_CHECK_TIME_US > 0)
    uint64_t end = time_us_64() + MAPLE_OPEN_LINE_CHECK_TIME_US + 1;
    while (time_us_64() < end) {
        if ((gpio_get_all() & mMaskAB) != mMaskAB) return false;
    }
#endif
    return true;
}

void MapleBus::setDirection(bool output) {
    if (!output) {
        gpio_set_dir_in_masked(mMaskAB);
        gpio_set_function(mPinB, GPIO_FUNC_SIO);
        gpio_set_function(mPinA, GPIO_FUNC_SIO);
    }
    if (mDirPin >= 0)
        gpio_put((uint)mDirPin, mDirOutHigh ? output : !output);
}

void __not_in_flash_func(MapleBus::writeIsr)() {
    pio_interrupt_clear(mPio, mSmOut);
    pio_sm_set_enabled(mPio, mSmOut, false);
    if (!mExpectingResponse) {
        gpio_set_mask(mMaskAB);
        gpio_set_dir_out_masked(mMaskAB);
    } else {
        gpio_set_pulls(mPinB, false, true);
    }
    gpio_set_dir_in_masked(mMaskAB);
    gpio_set_function(mPinB, GPIO_FUNC_SIO);
    gpio_set_function(mPinA, GPIO_FUNC_SIO);

    if (mExpectingResponse) {
        /* The handoff. The bus is neutral and the peripheral will not start
         * answering for tens of microseconds, which is ample for this. */
        loadProgram(Loaded::IN);
        pio_sm_init(mPio, mSmIn, MAPLE_PROGRAM_OFFSET, &mCfgIn);
        pio_sm_clear_fifos(mPio, mSmIn);
        pio_sm_set_consecutive_pindirs(mPio, mSmIn, mPinA, 2, false);
        pio_gpio_init(mPio, mPinA);
        pio_gpio_init(mPio, mPinB);
        pio_sm_set_enabled(mPio, mSmIn, true);
        setDirection(false);
        gpio_set_pulls(mPinB, true, false);
        mProcKillTime = (mResponseTimeoutUs == UINT64_MAX) ? UINT64_MAX : (time_us_64() + mResponseTimeoutUs);
        mCurrentPhase = Phase::WAITING_FOR_READ_START;
    } else {
        setDirection(false);
        mCurrentPhase = Phase::WRITE_COMPLETE;
    }
}

void __not_in_flash_func(MapleBus::readIsr)() {
    pio_interrupt_clear(mPio, mSmIn);
    if (mCurrentPhase == Phase::WAITING_FOR_READ_START) {
        mCurrentPhase = Phase::READ_IN_PROGRESS;
        mLastReceivedWordTimeUs = time_us_64();
    } else if (mCurrentPhase == Phase::READ_IN_PROGRESS) {
        pio_sm_set_enabled(mPio, mSmIn, false);
        gpio_set_dir_in_masked(mMaskAB);
        gpio_set_function(mPinB, GPIO_FUNC_SIO);
        gpio_set_function(mPinA, GPIO_FUNC_SIO);
        mCurrentPhase = Phase::READ_COMPLETE;
    }
}

bool MapleBus::write(const MaplePacket& packet, bool autostartRead, uint64_t readTimeoutUs, MaplePacket::ByteOrder rxByteOrder) {
    if (mCurrentPhase != Phase::IDLE) return false;

    dma_channel_abort(mDmaWriteChannel);
    dma_channel_abort(mDmaReadChannel);

    uint8_t crc = 0;
    uint32_t frameWord = packet.getFrameWord();
    crc8_word(frameWord, crc);
    for (unsigned i = 0; i < packet.payloadCount; i++) crc8_word(packet.payload[i], crc);

    bool flipBytes = (packet.payloadByteOrder != MaplePacket::ByteOrder::NETWORK);
    uint32_t totalBits = MaplePacket::getNumTotalBits((uint32_t)packet.payloadCount);
    unsigned len = 0;
    mWriteBuffer[len++] = flipBytes ? MaplePacket::flipWordBytes(totalBits) : totalBits;
    mWriteBuffer[len++] = frameWord;
    for (unsigned i = 0; i < packet.payloadCount; i++)
        mWriteBuffer[len++] = packet.payload[i];
    mWriteBuffer[len++] = flipBytes ? (uint32_t)crc : ((uint32_t)crc << 24);

    if (!lineCheck()) return false;

    mExpectingResponse = autostartRead;
    mResponseTimeoutUs = readTimeoutUs;
    mRxByteOrder = rxByteOrder;
    mCurrentPhase = Phase::WRITE_IN_PROGRESS;

    /* Arm the read DMA up front. The read state machine cannot be armed here
     * too, because the block still holds maple_out; writeIsr() does that after
     * the swap. Arming DMA is the expensive half and it does not depend on
     * which program is resident. */
    if (autostartRead) {
        /* Drain anything left in the read FIFO before arming DMA. The read
         * state machine is not configured until writeIsr() swaps the program
         * in, so a stale word here would otherwise be pulled straight into the
         * buffer and shift the whole response. */
        pio_sm_set_enabled(mPio, mSmIn, false);
        pio_sm_clear_fifos(mPio, mSmIn);
        mLastReadTransferCount = sizeof(mReadBuffer) / sizeof(mReadBuffer[0]);
        dma_channel_transfer_to_buffer_now(mDmaReadChannel, mReadBuffer, mLastReadTransferCount);
    }

    setDirection(true);
    loadProgram(Loaded::OUT);
    pio_sm_init(mPio, mSmOut, MAPLE_PROGRAM_OFFSET, &mCfgOut);
    pio_sm_clear_fifos(mPio, mSmOut);
    pio_sm_set_consecutive_pindirs(mPio, mSmOut, mPinA, 2, false);
    pio_gpio_init(mPio, mPinA);
    pio_gpio_init(mPio, mPinB);
    pio_sm_set_enabled(mPio, mSmOut, true);
    dma_channel_transfer_from_buffer_now(mDmaWriteChannel, mWriteBuffer, len);

    uint32_t txNs = MaplePacket::getTxTimeNs((uint32_t)packet.payloadCount, MAPLE_NS_PER_BIT);
    txNs = txNs * (100 + MAPLE_WRITE_TIMEOUT_EXTRA_PERCENT) / 100;
    mProcKillTime = time_us_64() + MAPLE_INT_DIVIDE_CEILING(txNs, 1000);
    return true;
}

MapleBus::Status MapleBus::processEvents(uint64_t currentTimeUs) {
    Status st;
    st.phase = mCurrentPhase;
    st.failureReason = FailureReason::NONE;
    st.readBuffer = nullptr;
    st.readBufferLen = 0;
    st.rxByteOrder = mRxByteOrder;

    if (st.phase == Phase::READ_COMPLETE) {
        uint64_t t0 = time_us_64();
        while (!pio_sm_is_rx_fifo_empty(mPio, mSmIn) && (time_us_64() - t0 < 1000))
            tight_loop_contents();
        uint32_t total = sizeof(mReadBuffer) / sizeof(mReadBuffer[0]);
        uint32_t left = dma_channel_hw_addr(mDmaReadChannel)->transfer_count;
        uint32_t dmaWordsRead = total - left;

        if (dmaWordsRead > 1) {
            uint8_t len = (mRxByteOrder != MaplePacket::ByteOrder::NETWORK)
                ? (mReadBuffer[0] & 0xFF) : (mReadBuffer[0] >> 24);
            uint8_t expectedCrc = (mRxByteOrder != MaplePacket::ByteOrder::NETWORK)
                ? (mReadBuffer[dmaWordsRead - 1] & 0xFF) : (mReadBuffer[dmaWordsRead - 1] >> 24);

            if (len <= dmaWordsRead - 2) {
                for (uint32_t i = 0; i < dmaWordsRead - 1; i++) mLastRead[i] = mReadBuffer[i];
                uint8_t crc = 0;
                crc8_words(mLastRead, dmaWordsRead - 1, crc);
                if (crc == expectedCrc) {
                    st.readBuffer = mLastRead;
                    st.readBufferLen = dmaWordsRead - 1;
                } else {
                    st.phase = Phase::READ_FAILED;
                    st.failureReason = FailureReason::CRC_INVALID;
                }
            } else {
                st.phase = Phase::READ_FAILED;
                st.failureReason = FailureReason::MISSING_DATA;
            }
        } else {
            st.phase = Phase::READ_FAILED;
            st.failureReason = FailureReason::MISSING_DATA;
        }
        mCurrentPhase = Phase::IDLE;
        return st;
    }

    if (st.phase == Phase::WRITE_COMPLETE) {
        mCurrentPhase = Phase::IDLE;
        return st;
    }

    if (st.phase == Phase::READ_IN_PROGRESS) {
        uint32_t tc = dma_channel_hw_addr(mDmaReadChannel)->transfer_count;
        if (tc == 0) {
            st.phase = Phase::READ_FAILED;
            st.failureReason = FailureReason::BUFFER_OVERFLOW;
            mCurrentPhase = Phase::IDLE;
        } else if (tc == mLastReadTransferCount) {
            if (currentTimeUs > mLastReceivedWordTimeUs &&
                (currentTimeUs - mLastReceivedWordTimeUs) >= MAPLE_INTER_WORD_READ_TIMEOUT_US) {
                pio_sm_set_enabled(mPio, mSmIn, false);
                st.phase = Phase::READ_FAILED;
                st.failureReason = FailureReason::TIMEOUT;
                mCurrentPhase = Phase::IDLE;
            }
        } else {
            mLastReadTransferCount = tc;
            mLastReceivedWordTimeUs = currentTimeUs;
        }
        return st;
    }

    if (st.phase != Phase::IDLE && currentTimeUs >= mProcKillTime) {
        if (st.phase == Phase::WAITING_FOR_READ_START) {
            pio_sm_set_enabled(mPio, mSmIn, false);
            st.phase = Phase::READ_FAILED;
            st.failureReason = FailureReason::TIMEOUT;
        } else {
            pio_sm_set_enabled(mPio, mSmOut, false);
            pio_sm_set_enabled(mPio, mSmIn, false);
            setDirection(false);
            st.phase = Phase::WRITE_FAILED;
            st.failureReason = FailureReason::TIMEOUT;
        }
        mCurrentPhase = Phase::IDLE;
    }
    return st;
}
