/*
 * Copyright (C) 2019 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup net_gnrc_lorawan
 * @{
 *
 * @file
 * @brief   GNRC LoRaWAN internal header
 *
 * @author  Jose Ignacio Alamos <jose.alamos@haw-hamburg.de>
 */
#ifndef GNRC_LORAWAN_INTERNAL_H
#define GNRC_LORAWAN_INTERNAL_H

#include <stdio.h>
#include <string.h>
#include "iolist.h"
#include "net/lora.h"
#include "net/lorawan/hdr.h"
#include "net/loramac.h"
#include "gnrc_lorawan/lorawan.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MTYPE_MASK           0xE0                       /**< MHDR mtype mask */
#define MTYPE_JOIN_REQUEST   0x0                        /**< Join Request type */
#define MTYPE_JOIN_ACCEPT    0x1                        /**< Join Accept type */
#define MTYPE_UNCNF_UPLINK   0x2                        /**< Unconfirmed uplink type */
#define MTYPE_UNCNF_DOWNLINK 0x3                        /**< Unconfirmed downlink type */
#define MTYPE_CNF_UPLINK     0x4                        /**< Confirmed uplink type */
#define MTYPE_CNF_DOWNLINK   0x5                        /**< Confirmed downlink type */
#define MTYPE_REJOIN_REQ     0x6                        /**< Re-join request type */
#define MTYPE_PROPIETARY     0x7                        /**< Propietary frame type */

#define MAJOR_MASK     0x3                              /**< Major mtype mask */
#define MAJOR_LRWAN_R1 0x0                              /**< LoRaWAN R1 version type */

#define JOIN_REQUEST_SIZE (23U)                         /**< Join Request size in bytes */
#define MIC_SIZE (4U)                                   /**< MIC size in bytes */
#define CFLIST_SIZE (16U)                               /**< Channel Frequency list size in bytes */

#define LORAWAN_STATE_IDLE (0)                          /**< MAC state machine in idle */
#define LORAWAN_STATE_RX_1 (1)                          /**< MAC state machine in RX1 */
#define LORAWAN_STATE_RX_2 (2)                          /**< MAC state machine in RX2 */
#define LORAWAN_STATE_TX (3)                            /**< MAC state machine in TX */

#define GNRC_LORAWAN_DIR_UPLINK (0U)                    /**< uplink frame direction */
#define GNRC_LORAWAN_DIR_DOWNLINK (1U)                  /**< downlink frame direction */

#define GNRC_LORAWAN_BACKOFF_BUDGET_1   (36000000LL)    /**< budget of time on air during the first hour */
#define GNRC_LORAWAN_BACKOFF_BUDGET_2   (36000000LL)    /**< budget of time on air between 1-10 hours after boot */
#define GNRC_LORAWAN_BACKOFF_BUDGET_3   (8700000LL)     /**< budget of time on air every 24 hours */

#define GNRC_LORAWAN_MLME_OPTS_LINK_CHECK_REQ  (1 << 0) /**< Internal Link Check request flag */

#define GNRC_LORAWAN_CID_SIZE (1U)                      /**< size of Command ID in FOps */
#define GNRC_LORAWAN_CID_LINK_CHECK_REQ_ANS (0x02)      /**< Link Check CID */

#define GNRC_LORAWAN_FOPT_LINK_ANS_SIZE (3U)            /**< size of Link check answer */

#define GNRC_LORAWAN_JOIN_DELAY_U32_MASK (0x1FFFFF)     /**< mask for detecting overflow in frame counter */

#define GNRC_LORAWAN_MAX_PAYLOAD_1 (59U)                /**< max MAC payload in DR0, DR1 and DR2 */
#define GNRC_LORAWAN_MAX_PAYLOAD_2 (123U)               /**< max MAC payload in DR3 */
#define GNRC_LORAWAN_MAX_PAYLOAD_3 (250U)               /**< max MAC payload above DR3 */

#define GNRC_LORAWAN_CFLIST_ENTRY_SIZE (3U)             /**< size of Channel Frequency list */
#define GNRC_LORAWAN_JOIN_ACCEPT_MAX_SIZE (33U)         /**< max size of Join Accept frame */

#define GNRC_LORAWAN_BACKOFF_STATE_1 (0U)               /**< backoff state during the first hour after boot */
#define GNRC_LORAWAN_BACKOFF_STATE_2 (1U)               /**< backoff state between 1-10 hours after boot */
#define GNRC_LORAWAN_BACKOFF_STATE_3 (2U)               /**< backoff state past 11 hours after boot */

#define GNRC_LORAWAN_BACKOFF_TIME_1 (1U)                /**< duration of first backoff state (in hours) */
#define GNRC_LORAWAN_BACKOFF_TIME_2 (10U)               /**< duration of second backoff state (in hours) */
#define GNRC_LORAWAN_BACKOFF_TIME_3 (24U)               /**< duration of third backoff state (in hours) */

#define GNRC_LORAWAN_APP_NONCE_SIZE (3U)                /**< App Nonce size */
#define GNRC_LORAWAN_NET_ID_SIZE (3U)                   /**< Net ID size */
#define GNRC_LORAWAN_DEV_NONCE_SIZE (2U)                /**< Dev Nonce size */

/**
 * @brief buffer helper for parsing and constructing LoRaWAN packets.
 */
typedef struct {
    uint8_t *data;  /**< pointer to the beginning of the buffer holding data */
    uint8_t size;   /**< size of the buffer */
    uint8_t index;  /**< current inxed in the buffer */
} lorawan_buffer_t;

/**
 * @brief Encrypts LoRaWAN payload
 *
 * @note This function is also used for decrypting a LoRaWAN packet. The LoRaWAN server encrypts the packet using decryption, so the end device only needs to implement encryption
 *
 * @param[in] iolist packet iolist representation
 * @param[in] dev_addr device address
 * @param[in] fcnt frame counter
 * @param[in] dir direction of the packet (0 if uplink, 1 if downlink)
 * @param[in] appskey pointer to the Application Session Key
 */
void gnrc_lorawan_encrypt_payload(gnrc_lorawan_t *mac, uint8_t *buf, size_t len, const le_uint32_t *dev_addr, uint32_t fcnt, uint8_t dir, const uint8_t *appskey);

/**
 * @brief Decrypts join accept message
 *
 * @param[in] key key to be used in the decryption
 * @param[in] pkt pointer to Join Accept MAC component (next byte after the MHDR)
 * @param[in] has_clist true if the Join Accept frame has CFList
 * @param[out] out buffer where the decryption is stored
 */
void gnrc_lorawan_decrypt_join_accept(gnrc_lorawan_t *mac, const uint8_t *key, uint8_t *pkt, int has_clist, uint8_t *out);

/**
 * @brief Generate LoRaWAN session keys
 *
 * Intended to be called after a successfull Join Request in order to generate
 * NwkSKey and AppSKey
 *
 * @param[in] app_nonce pointer to the app_nonce of the Join Accept message
 * @param[in] dev_nonce pointer to the dev_nonce buffer
 * @param[in] appkey pointer to eh AppKey
 * @param[out] nwkskey pointer to the NwkSKey
 * @param[out] appskey pointer to the AppSKey
 */
void gnrc_lorawan_generate_session_keys(gnrc_lorawan_t *mac, const uint8_t *app_nonce, const uint8_t *dev_nonce, const uint8_t *appkey, uint8_t *nwkskey, uint8_t *appskey);

/**
 * @brief Set datarate for the next transmission
 *
 * @param[in] mac pointer to the MAC descriptor
 * @param[in] datarate desired datarate
 *
 * @return 0 on success
 * @return -EINVAL if datarate is not available in the current region
 */
int gnrc_lorawan_set_dr(gnrc_lorawan_t *mac, uint8_t datarate);

/**
 * @brief build uplink frame
 *
 * @param[in] mac pointer to MAC descriptor
 * @param[in] payload packet containing payload
 * @param[in] confirmed_data true if confirmed frame
 * @param[in] port MAC port
 *
 * @return full LoRaWAN frame including payload
 * @return NULL if packet buffer is full. `payload` is released
 */
size_t gnrc_lorawan_build_uplink(gnrc_lorawan_t *mac, iolist_t *payload, int confirmed_data, uint8_t port,
        uint8_t *out);

/**
 * @brief pick a random available LoRaWAN channel
 *
 * @param[in] mac pointer to the MAC descriptor
 *
 * @return a free channel
 */
uint32_t gnrc_lorawan_pick_channel(gnrc_lorawan_t *mac);

/**
 * @brief Build fopts header
 *
 * @param[in] mac pointer to MAC descriptor
 * @param[out] buf destination buffer of fopts. If NULL, this function just returns
 *             the size of the expected fopts frame.
 *
 * @return size of the fopts frame
 */
uint8_t gnrc_lorawan_build_options(gnrc_lorawan_t *mac, lorawan_buffer_t *buf);

/**
 * @brief Process an fopts frame
 *
 * @param[in] mac pointer to MAC descriptor
 * @param[in] fopts pointer to fopts frame
 * @param[in] size size of fopts frame
 */
void gnrc_lorawan_process_fopts(gnrc_lorawan_t *mac, uint8_t *fopts, size_t size);

/**
 * @brief calculate join Message Integrity Code
 *
 * @param[in] io iolist representation of the packet
 * @param[in] key key used to calculate the MIC
 * @param[out] out calculated MIC
 */
void  gnrc_lorawan_calculate_join_mic(gnrc_lorawan_t *mac, const uint8_t *buf, size_t len, const uint8_t *key, le_uint32_t *out);

/**
 * @brief Calculate Message Integrity Code for a MCPS message
 *
 * @param[in] dev_addr the Device Address
 * @param[in] fcnt frame counter
 * @param[in] dir direction of the packet (0 is uplink, 1 is downlink)
 * @param[in] pkt the pkt
 * @param[in] nwkskey pointer to the Network Session Key
 * @param[out] out calculated MIC
 */
void gnrc_lorawan_calculate_mic(gnrc_lorawan_t *mac, const le_uint32_t *dev_addr, uint32_t fcnt,
                                uint8_t dir, uint8_t *buf, size_t len,
                                const uint8_t *nwkskey, le_uint32_t *out);

/**
 * @brief Build a MCPS LoRaWAN header
 *
 * @param[in] mtype the MType of the header
 * @param[in] dev_addr the Device Address
 * @param[in] fcnt frame counter
 * @param[in] ack true if ACK bit is set
 * @param[in] fopts_length the length of the FOpts field
 * @param[out] buf destination buffer of the hdr
 *
 * @return the size of the header
 */
size_t gnrc_lorawan_build_hdr(uint8_t mtype, le_uint32_t *dev_addr, uint32_t fcnt, uint8_t ack, uint8_t fopts_length, lorawan_buffer_t *buf);

/**
 * @brief Process an MCPS downlink message (confirmable or non comfirmable)
 *
 * @param[in] mac pointer to the MAC descriptor
 * @param[in] buf pointer to the downlink message
 * @param[in] len size of the downlink message
 */
void gnrc_lorawan_mcps_process_downlink(gnrc_lorawan_t *mac, uint8_t *buf,
        size_t len);

/**
 * @brief Init regional channel settings.
 *
 *        Intended to be called upon initialization
 *
 * @param[in] mac pointer to the MAC descriptor
 */
void gnrc_lorawan_channels_init(gnrc_lorawan_t *mac);

/**
 * @brief Reset MAC parameters
 *
 * @note This doesn't affect backoff timers variables.
 *
 * @param[in] mac pointer to the MAC layer
 */
void gnrc_lorawan_reset(gnrc_lorawan_t *mac);

/**
 * @brief Send a LoRaWAN packet
 *
 * @param[in] mac pointer to the MAC descriptor
 * @param[in] io the packet to be sent
 * @param[in] dr the datarate used for the transmission
 */
void gnrc_lorawan_send_pkt(gnrc_lorawan_t *mac, iolist_t *io, uint8_t dr);

/**
 * @brief Process join accept message
 *
 * @param[in] mac pointer to the MAC descriptor
 * @param[in] data pointer to the Join Accept packet
 * @param[in] size size of the Join Accept packet
 */
void gnrc_lorawan_mlme_process_join(gnrc_lorawan_t *mac, uint8_t *data, size_t size);

/**
 * @brief Inform the MAC layer that no packet was received during reception.
 *
 *        To be called when the radio reports "NO RX" after the second reception
 *        window
 *
 * @param[in] mac pointer to the MAC descriptor
 */
void gnrc_lorawan_mlme_no_rx(gnrc_lorawan_t *mac);

/**
 * @brief Trigger a MCPS event
 *
 * @param[in] mac pointer to the MAC descriptor
 * @param[in] event the event to be processed.
 * @param[in] data set to true if the packet contains payload
 */
void gnrc_lorawan_mcps_event(gnrc_lorawan_t *mac, int event, int data);

/**
 * @brief Get the maximum MAC payload (M value) for a given datarate.
 *
 * @note This function is region specific
 *
 * @param[in] datarate datarate
 *
 * @return the maximum allowed size of the packet
 */
uint8_t gnrc_lorawan_region_mac_payload_max(uint8_t datarate);

/**
 * @brief Open a reception window
 *
 *        This is called by the MAC layer on timeout event.
 *
 * @param[in] mac pointer to the MAC descriptor
 */
void gnrc_lorawan_open_rx_window(gnrc_lorawan_t *mac);

/**
 * @brief save internal MAC state in non-volatile storage and shutdown
 *        the MAC layer gracefully.
 *
 * @param mac
 */
void gnrc_lorawan_perform_save(gnrc_lorawan_t *mac);

/**
 * @brief Acquire the MAC layer
 *
 * @param[in] mac pointer to the MAC descriptor
 *
 * @return true on success
 * @return false if MAC is already acquired
 */
static inline int gnrc_lorawan_mac_acquire(gnrc_lorawan_t *mac)
{
    int _c = mac->busy;

    mac->busy = true;
    return !_c;
}

/**
 * @brief Release the MAC layer
 *
 * @param[in] mac
 */
static inline void gnrc_lorawan_mac_release(gnrc_lorawan_t *mac)
{
    mac->busy = false;
}

void gnrc_lorawan_set_rx2_dr(gnrc_lorawan_t *mac, uint8_t rx2_dr);

#ifdef __cplusplus
}
#endif

#endif /* GNRC_LORAWAN_INTERNAL_H */
/** @} */
