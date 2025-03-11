#include "srsran_exports.h"
#include "srsran/phy/phch/pbch_nr.h"
#include "srsran/phy/common/sequence.h"
#include "srsran/phy/fec/polar/polar_chanalloc.h"
#include "srsran/phy/fec/polar/polar_interleaver.h"
#include "srsran/phy/mimo/precoding.h"
#include "srsran/phy/modem/demod_soft.h"
#include "srsran/phy/modem/mod.h"
#include "srsran/phy/utils/debug.h"
#include "srsran/phy/utils/simd.h"
#include "srsran/phy/utils/vector.h"

#define PBCH_SFN_PAYLOAD_BEGIN 1
#define PBCH_SFN_PAYLOAD_LENGTH 6
#define PBCH_SFN_2ND_LSB_G (G[PBCH_SFN_PAYLOAD_LENGTH + 2])
#define PBCH_SFN_3RD_LSB_G (G[PBCH_SFN_PAYLOAD_LENGTH + 1])

#define PBCH_NR_DEBUG_TX(...) DEBUG("PBCH-NR Tx: " __VA_ARGS__)
#define PBCH_NR_DEBUG_RX(...) DEBUG("PBCH-NR Rx: " __VA_ARGS__)

/*
 * CRC Parameters
 */
#define PBCH_NR_CRC SRSRAN_LTE_CRC24C
#define PBCH_NR_CRC_LEN 24

/*
 * Polar code N_max
 */
#define PBCH_NR_POLAR_N_MAX 9U

/*
 * Polar rate matching I_BIL
 */
#define PBCH_NR_POLAR_RM_IBIL 0

/*
 * Number of generated payload bits, called A
 */
#define PBCH_NR_A (SRSRAN_PBCH_MSG_NR_SZ + 8)

/*
 * Number of payload bits plus CRC
 */
#define PBCH_NR_K (PBCH_NR_A + PBCH_NR_CRC_LEN)

/*
 * Number of Polar encoded bits
 */
#define PBCH_NR_N (1U << PBCH_NR_POLAR_N_MAX)

/*
 * Number of RM bits
 */
#define PBCH_NR_E (864)

/*
 * Number of symbols
 */
#define PBCH_NR_M (PBCH_NR_E / 2)

const uint32_t G[PBCH_NR_A] = {16, 23, 18, 17, 8,  30, 10, 6,  24, 7,  0,  5,  3,  2,  1,  4,
                                      9,  11, 12, 13, 14, 15, 19, 20, 21, 22, 25, 26, 27, 28, 29, 31};


void pbch_nr_pbch_msg_unpack(const srsran_pbch_nr_cfg_t* cfg, const uint8_t a[PBCH_NR_A], srsran_pbch_msg_nr_t* msg)
{
  if (get_srsran_verbose_level() >= SRSRAN_VERBOSE_DEBUG && !is_handler_registered()) {
    PBCH_NR_DEBUG_RX("Packed PBCH bits: ");
    srsran_vec_fprint_byte(stdout, a, PBCH_NR_A);
  }

  // Extract actual payload size
  uint32_t A_hat = SRSRAN_PBCH_MSG_NR_SZ;

  // Get actual payload
  uint32_t j_sfn   = 0;
  uint32_t j_other = 14;
  for (uint32_t i = 0; i < A_hat; i++) {
    if (i >= PBCH_SFN_PAYLOAD_BEGIN && i < (PBCH_SFN_PAYLOAD_BEGIN + PBCH_SFN_PAYLOAD_LENGTH)) {
      msg->payload[i] = a[G[j_sfn++]];
    } else {
      msg->payload[i] = a[G[j_other++]];
    }
  }

  // Put SFN in a_hat[A_hat] to a_hat[A_hat + 3]
  msg->sfn_4lsb = 0;
  msg->sfn_4lsb |= (uint8_t)(a[G[j_sfn++]] << 3U); // 4th LSB of SFN
  msg->sfn_4lsb |= (uint8_t)(a[G[j_sfn++]] << 2U); // 3th LSB of SFN
  msg->sfn_4lsb |= (uint8_t)(a[G[j_sfn++]] << 1U); // 2th LSB of SFN
  msg->sfn_4lsb |= (uint8_t)(a[G[j_sfn++]] << 0U); // 1th LSB of SFN

  // Put HRF in a_hat[A_hat + 4]
  msg->hrf = (a[G[10]] == 1);

  // Put SSB related in a_hat[A_hat + 5] to a_hat[A_hat + 7]
  msg->ssb_idx = cfg->ssb_idx; // Load 4 LSB
  if (cfg->Lmax == 64) {
    msg->ssb_idx = msg->ssb_idx & 0b111;
    msg->ssb_idx |= (uint8_t)(a[G[11]] << 5U); // 6th bit of SSB index
    msg->ssb_idx |= (uint8_t)(a[G[12]] << 4U); // 5th bit of SSB index
    msg->ssb_idx |= (uint8_t)(a[G[13]] << 3U); // 4th bit of SSB index
  } else {
    msg->k_ssb_msb = a[G[11]];
  }
}

void pbch_nr_scramble(const srsran_pbch_nr_cfg_t* cfg, const uint8_t a[PBCH_NR_A], uint8_t a_prime[PBCH_NR_A])
{
  uint32_t i = 0;
  uint32_t j = 0;

  // Initialise sequence
  srsran_sequence_state_t sequence_state = {};
  srsran_sequence_state_init(&sequence_state, SRSRAN_SEQUENCE_MOD(cfg->N_id));

  // Select value M
  uint32_t M = PBCH_NR_A - 3;
  if (cfg->Lmax == 64) {
    M = PBCH_NR_A - 6;
  }

  // Select value v
  uint32_t v = 2 * a[PBCH_SFN_3RD_LSB_G] + a[PBCH_SFN_2ND_LSB_G];

  // Advance sequence
  srsran_sequence_state_advance(&sequence_state, M * v);

  // Generate actual sequence
  uint8_t c[PBCH_NR_A] = {};
  srsran_sequence_state_apply_bit(&sequence_state, c, c, PBCH_NR_A);

  while (i < PBCH_NR_A) {
    uint8_t s_i = c[j];

    // Check if i belongs to a SS/PBCH block index which is only multiplexed when L_max is 64
    bool is_ssb_idx = (i == G[11] || i == G[12] || i == G[13]) && cfg->Lmax == 64;

    // a i corresponds to any one of the bits belonging to the SS/PBCH block index, the half frame index, and 2 nd and 3
    // rd least significant bits of the system frame number
    if (is_ssb_idx || i == G[10] || i == PBCH_SFN_2ND_LSB_G || i == PBCH_SFN_3RD_LSB_G) {
      s_i = 0;
    } else {
      j++;
    }

    a_prime[i] = a[i] ^ s_i;
    i++;
  }
}

int pbch_nr_polar_rm_rx(srsran_pbch_nr_t* q, const int8_t llr[PBCH_NR_E], int8_t d[PBCH_NR_N])
{
  if (srsran_polar_rm_rx_c(&q->polar_rm_rx, llr, d, PBCH_NR_E, q->code.n, PBCH_NR_K, PBCH_NR_POLAR_RM_IBIL) <
      SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }

  // Negate all LLR
  for (uint32_t i = 0; i < PBCH_NR_N; i++) {
    d[i] *= -1;
  }

  if (get_srsran_verbose_level() >= SRSRAN_VERBOSE_DEBUG && !is_handler_registered()) {
    PBCH_NR_DEBUG_RX("d: ");
    srsran_vec_fprint_bs(stdout, d, PBCH_NR_N);
  }

  return SRSRAN_SUCCESS;
}

int pbch_nr_polar_decode(srsran_pbch_nr_t* q, const int8_t d[PBCH_NR_N], uint8_t c[PBCH_NR_K])
{
  // Decode bits
  uint8_t allocated[PBCH_NR_N];
  if (srsran_polar_decoder_decode_c(&q->polar_decoder, d, allocated, q->code.n, q->code.F_set, q->code.F_set_size) <
      SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }

  if (get_srsran_verbose_level() >= SRSRAN_VERBOSE_DEBUG && !is_handler_registered()) {
    PBCH_NR_DEBUG_RX("Allocated: ");
    srsran_vec_fprint_byte(stdout, allocated, PBCH_NR_N);
  }

  // Allocate channel
  uint8_t c_prime[SRSRAN_POLAR_INTERLEAVER_K_MAX_IL];
  srsran_polar_chanalloc_rx(allocated, c_prime, q->code.K, q->code.nPC, q->code.K_set, q->code.PC_set);

  // Interleave
  srsran_polar_interleaver_run_u8(c_prime, c, PBCH_NR_K, false);

  return SRSRAN_SUCCESS;
}

int pbch_nr_polar_rm_tx(srsran_pbch_nr_t* q, const uint8_t d[PBCH_NR_N], uint8_t o[PBCH_NR_E])
{
  if (srsran_polar_rm_tx(&q->polar_rm_tx, d, o, q->code.n, PBCH_NR_E, PBCH_NR_K, PBCH_NR_POLAR_RM_IBIL) <
      SRSRAN_SUCCESS) {
    return SRSRAN_ERROR;
  }

  if (get_srsran_verbose_level() >= SRSRAN_VERBOSE_DEBUG && !is_handler_registered()) {
    PBCH_NR_DEBUG_TX("d: ");
    srsran_vec_fprint_byte(stdout, d, PBCH_NR_N);
  }

  return SRSRAN_SUCCESS;
}

void pbch_nr_scramble_rx(const srsran_pbch_nr_cfg_t* cfg,
                                uint32_t                    ssb_idx,
                                const int8_t                b_hat[PBCH_NR_E],
                                int8_t                      b[PBCH_NR_E])
{
  // Initialise sequence
  srsran_sequence_state_t sequence_state = {};
  srsran_sequence_state_init(&sequence_state, SRSRAN_SEQUENCE_MOD(cfg->N_id));

  // Select value M
  uint32_t M_bit = PBCH_NR_E;

  // Select value v
  // for L max = 8 or L max = 64 , & is the three least significant bits of the SS/PBCH block index
  uint32_t v = (ssb_idx & 0b111U);
  if (cfg->Lmax == 4) {
    // for L max = 4 , & is the two least significant bits of the SS/PBCH block index
    v = ssb_idx & 0b11U;
  }

  // Advance sequence
  srsran_sequence_state_advance(&sequence_state, v * M_bit);

  // Apply sequence
  srsran_sequence_state_apply_c(&sequence_state, b_hat, b, PBCH_NR_E);
}
