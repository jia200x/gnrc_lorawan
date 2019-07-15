/*
 * Copyright (C) 2017 Fundación Inria Chile
 * Copyright (C) 2019 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @defgroup    net_gnrc_lorawan GNRC LoRaWAN
 * @ingroup     net_gnrc
 * @brief       GNRC LoRaWAN stack implementation
 *
 * @{
 *
 * @file
 * @brief   GNRC LoRaWAN API definition
 *
 * @author  José Ignacio Alamos <jose.alamos@haw-hamburg.de>
 * @author  Francisco Molina <femolina@uc.cl>
 */
#ifndef NET_GNRC_LORAWAN_H
#define NET_GNRC_LORAWAN_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include "iolist.h"
#include "byteorder.h"
#include "net/lora.h"
#include "errno.h"

#define GNRC_LORAWAN_MAX_CHANNELS (16U)                 /**< Maximum number of channels */
#define GNRC_LORAWAN_BACKOFF_WINDOW_TICK (3600000000LL) /**< backoff expire tick in usecs (set to 1 second) */


/**
 * @brief maximum timer drift in percentage
 *
 * E.g a value of 0.1 means there's a positive drift of 0.1% (set timeout to
 * 1000 ms => triggers after 1001 ms)
 */
#ifndef CONFIG_GNRC_LORAWAN_TIMER_DRIFT
#define CONFIG_GNRC_LORAWAN_TIMER_DRIFT 1
#endif

/**
 * @brief the minimum symbols to detect a LoRa preamble
 */
#ifndef CONFIG_GNRC_LORAWAN_MIN_SYMBOLS_TIMEOUT
#define CONFIG_GNRC_LORAWAN_MIN_SYMBOLS_TIMEOUT 50
#endif

#define GNRC_LORAWAN_REQ_STATUS_SUCCESS (0)     /**< MLME or MCPS request successful status */
#define GNRC_LORAWAN_REQ_STATUS_DEFERRED (1)    /**< the MLME or MCPS confirm message is asynchronous */

/**
 * @brief MLME Join Request data
 */
typedef struct {
    void *deveui;   /**< pointer to the Device EUI */
    void *appeui;   /**< pointer to the Application EUI */
    void *appkey;   /**< pointer to the Application Key */
    uint8_t dr;     /**< datarate for the Join Request */
} mlme_lorawan_join_t;


/**
 * @brief MLME Link Check confirmation data
 */
typedef struct {
    uint8_t margin;         /**< demodulation margin (in dB) */
    uint8_t num_gateways;   /**< number of gateways */
} mlme_link_req_confirm_t;

/**
 * @brief MCPS data
 */
typedef struct {
    iolist_t *pkt;    /**< packet of the request */
    uint8_t port;           /**< port of the request */
    uint8_t dr;             /**< datarate of the request */
} mcps_data_t;

/**
 * @brief MCPS service access point descriptor
 */
typedef struct {
    uint32_t fcnt;                  /**< uplink framecounter */
    uint32_t fcnt_down;             /**< downlink frame counter */
    int nb_trials;              /**< holds the remaining number of retransmissions */
    int ack_requested;          /**< wether the network server requested an ACK */
    int waiting_for_ack;        /**< true if the MAC layer is waiting for an ACK */
} gnrc_lorawan_mcps_t;

/**
 * @brief MLME service access point descriptor
 */
typedef struct {
    uint8_t activation;         /**< Activation mechanism of the MAC layer */
    int pending_mlme_opts;  /**< holds pending mlme opts */
    uint32_t nid;               /**< current Network ID */
    int32_t backoff_budget;     /**< remaining Time On Air budget */
    uint8_t dev_nonce[2];       /**< Device Nonce */
    uint8_t backoff_state;      /**< state in the backoff state machine */
} gnrc_lorawan_mlme_t;

/**
 * @brief GNRC LoRaWAN mac descriptor */
typedef struct {
    gnrc_lorawan_mcps_t mcps;                       /**< MCPS descriptor */
    gnrc_lorawan_mlme_t mlme;                       /**< MLME descriptor */
    uint8_t *tx_buf;
    size_t tx_len;
    uint8_t *nwkskey;                               /**< pointer to Network SKey buffer */
    uint8_t *appskey;                               /**< pointer to Application SKey buffer */
    uint32_t channel[GNRC_LORAWAN_MAX_CHANNELS];    /**< channel array */
    uint32_t toa;                                   /**< Time on Air of the last transmission */
    int busy;                                       /**< MAC busy  */
    int shutdown_req;                               /**< MAC Shutdown request */
    le_uint32_t dev_addr;                           /**< Device address */
    int state;                                      /**< state of MAC layer */
    uint8_t dl_settings;                            /**< downlink settings */
    uint8_t rx_delay;                               /**< Delay of first reception window */
    uint8_t dr_range[GNRC_LORAWAN_MAX_CHANNELS];    /**< Datarate Range for all channels */
    uint8_t last_dr;                                /**< datarate of the last transmission */
} gnrc_lorawan_t;
/**
 * @brief MCPS events
 */
typedef enum {
    MCPS_EVENT_RX,            /**< MCPS RX event */
    MCPS_EVENT_NO_RX,         /**< MCPS no RX event */
} mcps_event_t;

/**
 * @brief LoRaWAN activation mechanism
 */
typedef enum {
    MLME_ACTIVATION_NONE,     /**< MAC layer is not activated */
    MLME_ACTIVATION_ABP,      /**< MAC layer activated by ABP */
    MLME_ACTIVATION_OTAA      /**< MAC layer activated by OTAA */
} mlme_activation_t;

/**
 * @brief MAC Information Base attributes
 */
typedef enum {
    MIB_ACTIVATION_METHOD,      /**< type is activation method */
    MIB_DEV_ADDR,               /**< type is dev addr */
    MIB_RX2_DR,                 /**< type is rx2 DR */
} mlme_mib_type_t;

/**
 * @brief MLME primitive types
 */
typedef enum {
    MLME_JOIN,                 /**< join a LoRaWAN network */
    MLME_LINK_CHECK,           /**< perform a Link Check */
    MLME_RESET,                /**< reset the MAC layer */
    MLME_SET,                  /**< set the MIB */
    MLME_GET,                  /**< get the MIB */
    MLME_SCHEDULE_UPLINK       /**< schedule uplink indication */
} mlme_type_t;

/**
 * @brief MCPS primitive types
 */
typedef enum {
    MCPS_CONFIRMED,            /**< confirmed data */
    MCPS_UNCONFIRMED           /**< unconfirmed data */
} mcps_type_t;

/**
 * @brief MAC Information Base descriptor for MLME Request-Confirm
 */
typedef struct {
    mlme_mib_type_t type; /**< MIB attribute identifier */
    union {
        mlme_activation_t activation;   /**< holds activation mechanism */
        void *dev_addr;               /**< pointer to the dev_addr */
        uint8_t rx2_dr;
    };
} mlme_mib_t;

/**
 * @brief MAC (sub) Layer Management Entity (MLME) request representation
 */
typedef struct {
    union {
        mlme_lorawan_join_t join; /**< Join Data holder */
        mlme_mib_t mib;           /**< MIB holder */
    };
    mlme_type_t type;   /**< type of the MLME request */
} mlme_request_t;

/**
 * @brief Mac Common Part Sublayer (MCPS) request representation
 */
typedef struct {
    union {
        mcps_data_t data;        /**< MCPS data holder */
    };
    mcps_type_t type;    /**< type of the MCPS request */
} mcps_request_t;

/**
 * @brief MAC (sub) Layer Management Entity (MLME) confirm representation
 */
typedef struct {
    int16_t status; /**< status of the MLME confirm */
    mlme_type_t type;   /**< type of the MLME confirm */
    union {
        mlme_link_req_confirm_t link_req; /**< Link Check confirmation data */
        mlme_mib_t mib;                   /**< MIB confirmation data */
    };
} mlme_confirm_t;

/**
 * @brief Mac Common Part Sublayer (MCPS) confirm representation
 */
typedef struct {
    void *data;     /**< data of the MCPS confirm */
    int16_t status; /**< status of the MCPS confirm */
    mcps_type_t type;   /**< type of the MCPS confirm */
} mcps_confirm_t;

/**
 * @brief Mac Common Part Sublayer (MCPS) indication representation
 */
typedef struct {
    mcps_type_t type; /**< type of the MCPS indication */
    union {
        mcps_data_t data; /**< MCPS Data holder */
    };
} mcps_indication_t;

/**
 * @brief MAC (sub) Layer Management Entity (MLME) indication representation
 */
typedef struct {
    mlme_type_t type; /**< type of the MLME indication */
} mlme_indication_t;

/**
 * @brief Indicate the MAC layer there was a timeout event
 *
 * @param[in] mac pointer to the MAC descriptor
 */
void gnrc_lorawan_event_timeout(gnrc_lorawan_t *mac);

/**
 * @brief Indicate the MAC layer when the transmission finished
 *
 * @param[in] mac pointer to the MAC descriptor
 */
void gnrc_lorawan_event_tx_complete(gnrc_lorawan_t *mac);

/**
 * @brief Init GNRC LoRaWAN
 *
 * @param[in] mac pointer to the MAC descriptor
 * @param[in] nwkskey buffer to store the NwkSKey. Should be at least 16 bytes long
 * @param[in] appskey buffer to store the AppsKey. Should be at least 16 bytes long
 */
void gnrc_lorawan_init(gnrc_lorawan_t *mac, uint8_t *nwkskey, uint8_t *appskey,
        uint8_t *tx_buf);

/**
 * @brief Perform a MLME request
 *
 * @param[in] mac pointer to the MAC descriptor
 * @param[in] mlme_request the MLME request
 * @param[out] mlme_confirm the MLME confirm. `mlme_confirm->status` could either
 *             be GNRC_LORAWAN_REQ_STATUS_SUCCESS if the request was OK,
 *             GNRC_LORAWAN_REQ_STATUS_DEFERRED if the confirmation is deferred
 *             or an standard error number
 */
void gnrc_lorawan_mlme_request(gnrc_lorawan_t *mac, const mlme_request_t *mlme_request,
                               mlme_confirm_t *mlme_confirm);

/**
 * @brief Perform a MCPS request
 *
 * @param[in] mac pointer to the MAC descriptor
 * @param[in] mcps_request the MCPS request
 * @param[out] mcps_confirm the MCPS confirm. `mlme_confirm->status` could either
 *             be GNRC_LORAWAN_REQ_STATUS_SUCCESS if the request was OK,
 *             GNRC_LORAWAN_REQ_STATUS_DEFERRED if the confirmation is deferred
 *             or an standard error number
 */
void gnrc_lorawan_mcps_request(gnrc_lorawan_t *mac, const mcps_request_t *mcps_request,
                               mcps_confirm_t *mcps_confirm);

/**
 * @brief MLME Backoff expiration tick
 *
 *        Should be called every hour in order to maintain the Time On Air budget.
 *
 * @param[in] mac pointer to the MAC descriptor
 */

void gnrc_lorawan_mlme_backoff_expire(gnrc_lorawan_t *mac);

/**
 * @brief Process and dispatch a full LoRaWAN packet
 *
 *        Intended to be called right after reception from the radio
 *
 * @param[in] mac pointer to the MAC descriptor
 * @param[in] data pointer to the received packet
 * @param[in] size size of the received packet
 */
void gnrc_lorawan_process_pkt(gnrc_lorawan_t *mac, uint8_t *data, size_t size);

/**
 * @brief Tell the MAC layer the timer was fired
 *
 * @param mac pointer to the MAC descriptor
 */
void gnrc_lorawan_timer_fired(gnrc_lorawan_t *mac);

void gnrc_lorawan_timer_stop(gnrc_lorawan_t *mac);
void gnrc_lorawan_timer_set(gnrc_lorawan_t *mac, uint32_t secs);
void gnrc_lorawan_timer_usleep(gnrc_lorawan_t *mac, uint32_t us);

void gnrc_lorawan_mcps_indication(gnrc_lorawan_t *mac, mcps_indication_t *ind);
void gnrc_lorawan_mlme_indication(gnrc_lorawan_t *mac, mlme_indication_t *ind);
void gnrc_lorawan_mcps_confirm(gnrc_lorawan_t *mac, mcps_confirm_t *confirm);
void gnrc_lorawan_mlme_confirm(gnrc_lorawan_t *mac, mlme_confirm_t *confirm);
uint32_t gnrc_lorawan_random_get(gnrc_lorawan_t *mac);
void gnrc_lorawan_radio_sleep(gnrc_lorawan_t *mac);
void gnrc_lorawan_radio_set_cr(gnrc_lorawan_t *mac, uint8_t cr);
void gnrc_lorawan_radio_set_syncword(gnrc_lorawan_t *mac, uint8_t syncword);
void gnrc_lorawan_radio_set_frequency(gnrc_lorawan_t *mac, uint32_t channel);
void gnrc_lorawan_radio_set_iq_invert(gnrc_lorawan_t *mac, int invert);
void gnrc_lorawan_radio_set_rx_symbol_timeout(gnrc_lorawan_t *mac, uint16_t timeout);
void gnrc_lorawan_radio_rx_on(gnrc_lorawan_t *mac);
void gnrc_lorawan_radio_set_sf(gnrc_lorawan_t *mac, uint8_t sf);
void gnrc_lorawan_radio_set_bw(gnrc_lorawan_t *mac, uint8_t bw);
void gnrc_lorawan_radio_send(gnrc_lorawan_t *mac, iolist_t *io);
void gnrc_lorawan_cmac_init(gnrc_lorawan_t *mac, const void *key);
void gnrc_lorawan_cmac_update(gnrc_lorawan_t *mac, const void *buf, size_t len);
void gnrc_lorawan_cmac_finish(gnrc_lorawan_t *mac, void *out);
void gnrc_lorawan_aes128_init(gnrc_lorawan_t *mac, const void *key);
void gnrc_lorawan_aes128_encrypt(gnrc_lorawan_t *mac, const void *in, void *out);

#ifdef __cplusplus
}
#endif

#endif /* NET_GNRC_LORAWAN_H */
/** @} */
