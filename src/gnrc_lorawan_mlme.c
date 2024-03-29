/*
 * Copyright (C) 2019 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @author  José Ignacio Alamos <jose.alamos@haw-hamburg.de>
 */
#include <stdio.h>
#include <string.h>
#include "net/lora.h"
#include "gnrc_lorawan/lorawan.h"
#include "gnrc_lorawan/region.h"
#include "gnrc_lorawan_internal.h"
#include "errno.h"

#include "net/lorawan/hdr.h"

#define ENABLE_DEBUG    (0)
#include "debug.h"

static int _buffer_reset(lorawan_buffer_t *buf, uint8_t *data, size_t length)
{
    if (!buf || !data || !length) {
        return -EINVAL;
    }

    buf->data = data;
    buf->size = length;
    buf->index = 0;
    return 0;
}

static int gnrc_lorawan_send_join_request(gnrc_lorawan_t *mac, uint8_t *deveui,
                                          uint8_t *appeui, uint8_t *appkey, uint8_t dr)
{
    /* Dev Nonce */
    uint32_t random_number = gnrc_lorawan_random_get(mac);

    mac->mlme.dev_nonce[0] = random_number & 0xFF;
    mac->mlme.dev_nonce[1] = (random_number >> 8) & 0xFF;

    /* build join request */
    uint8_t pkt[sizeof(lorawan_join_request_t)];
    lorawan_join_request_t *hdr = (lorawan_join_request_t*) pkt;

    hdr->mt_maj = 0;
    lorawan_hdr_set_mtype((lorawan_hdr_t *) hdr, MTYPE_JOIN_REQUEST);
    lorawan_hdr_set_maj((lorawan_hdr_t *) hdr, MAJOR_LRWAN_R1);

    le_uint64_t l_appeui = *((le_uint64_t *) appeui);
    le_uint64_t l_deveui = *((le_uint64_t *) deveui);

    hdr->app_eui = l_appeui;
    hdr->dev_eui = l_deveui;

    le_uint16_t l_dev_nonce = *((le_uint16_t *) mac->mlme.dev_nonce);
    hdr->dev_nonce = l_dev_nonce;

    gnrc_lorawan_calculate_join_mic(mac, pkt, JOIN_REQUEST_SIZE - MIC_SIZE, appkey, &hdr->mic);

    /* We need a random delay for join request. Otherwise there might be
     * network congestion if a group of nodes start at the same time */
    gnrc_lorawan_timer_usleep(mac, gnrc_lorawan_random_get(mac) & GNRC_LORAWAN_JOIN_DELAY_U32_MASK);

    iolist_t io = {
        .iol_base = pkt,
        .iol_len = sizeof(lorawan_join_request_t),
        .iol_next = NULL
    };

    gnrc_lorawan_send_pkt(mac, &io, dr);

    mac->mlme.backoff_budget -= mac->toa;

    return GNRC_LORAWAN_REQ_STATUS_DEFERRED;
}

void gnrc_lorawan_mlme_process_join(gnrc_lorawan_t *mac, uint8_t *data, size_t size)
{
    int status;
    mlme_confirm_t mlme_confirm;

    if (mac->mlme.activation != MLME_ACTIVATION_NONE) {
        status = -EBADMSG;
        goto out;
    }

    if (size != GNRC_LORAWAN_JOIN_ACCEPT_MAX_SIZE - CFLIST_SIZE &&
        size != GNRC_LORAWAN_JOIN_ACCEPT_MAX_SIZE) {
        status = -EBADMSG;
        goto out;
    }

    /* Substract 1 from join accept max size, since the MHDR was already read */
    uint8_t out[GNRC_LORAWAN_JOIN_ACCEPT_MAX_SIZE - 1];
    uint8_t has_cflist = (size - 1) >= CFLIST_SIZE;
    gnrc_lorawan_decrypt_join_accept(mac, mac->appskey, ((uint8_t *) data) + 1,
                                     has_cflist, out);
    memcpy(((uint8_t *) data) + 1, out, size - 1);

    le_uint32_t mic;
    le_uint32_t *expected_mic = (le_uint32_t *) (((uint8_t *) data) + size - MIC_SIZE);
    gnrc_lorawan_calculate_join_mic(mac, data, size - MIC_SIZE, mac->appskey, &mic);
    if (mic.u32 != expected_mic->u32) {
        DEBUG("gnrc_lorawan_mlme: wrong MIC.\n");
        status = -EBADMSG;
        goto out;
    }

    lorawan_join_accept_t *ja_hdr = (lorawan_join_accept_t *) data;
    gnrc_lorawan_generate_session_keys(mac, ja_hdr->app_nonce, mac->mlme.dev_nonce, mac->appskey, mac->nwkskey, mac->appskey);

    le_uint32_t le_nid;
    le_nid.u32 = 0;
    memcpy(&le_nid, ja_hdr->net_id, 3);
    mac->mlme.nid = byteorder_ntohl(byteorder_ltobl(le_nid));
    /* Copy devaddr */
    memcpy(&mac->dev_addr, ja_hdr->dev_addr, sizeof(mac->dev_addr));

    mac->dl_settings = ja_hdr->dl_settings;

    /* delay 0 maps to 1 second */
    mac->rx_delay = ja_hdr->rx_delay ? ja_hdr->rx_delay : 1;

    gnrc_lorawan_process_cflist(mac, out + sizeof(lorawan_join_accept_t) - 1);
    mac->mlme.activation = MLME_ACTIVATION_OTAA;
    status = GNRC_LORAWAN_REQ_STATUS_SUCCESS;

out:
    mlme_confirm.type = MLME_JOIN;
    mlme_confirm.status = status;

    gnrc_lorawan_mlme_confirm(mac, &mlme_confirm);
}

void gnrc_lorawan_mlme_backoff_expire(gnrc_lorawan_t *mac)
{
    uint8_t counter = mac->mlme.backoff_state & 0x1F;
    uint8_t state = mac->mlme.backoff_state >> 5;

    if (counter == 0) {
        switch (state) {
            case GNRC_LORAWAN_BACKOFF_STATE_1:
                counter = GNRC_LORAWAN_BACKOFF_TIME_1;
                state = GNRC_LORAWAN_BACKOFF_STATE_2;
                mac->mlme.backoff_budget = GNRC_LORAWAN_BACKOFF_BUDGET_1;
                break;
            case GNRC_LORAWAN_BACKOFF_STATE_2:
                counter = GNRC_LORAWAN_BACKOFF_TIME_2;
                state = GNRC_LORAWAN_BACKOFF_STATE_3;
                mac->mlme.backoff_budget = GNRC_LORAWAN_BACKOFF_BUDGET_2;
                break;
            case GNRC_LORAWAN_BACKOFF_STATE_3:
            default:
                counter = GNRC_LORAWAN_BACKOFF_TIME_3;
                mac->mlme.backoff_budget = GNRC_LORAWAN_BACKOFF_BUDGET_3;
                break;
        }

    }

    counter--;
    mac->mlme.backoff_state = state << 5 | (counter & 0x1F);
}

static void _mlme_set(gnrc_lorawan_t *mac, const mlme_request_t *mlme_request,
                               mlme_confirm_t *mlme_confirm)
{
    mlme_confirm->status = -EINVAL;
    switch(mlme_request->mib.type) {
        case MIB_ACTIVATION_METHOD:
            if(mlme_request->mib.activation != MLME_ACTIVATION_OTAA) {
                mlme_confirm->status = GNRC_LORAWAN_REQ_STATUS_SUCCESS;
                mac->mlme.activation = mlme_request->mib.activation;
            }
            break;
        case MIB_DEV_ADDR:
            mlme_confirm->status = GNRC_LORAWAN_REQ_STATUS_SUCCESS;
            memcpy(&mac->dev_addr, mlme_request->mib.dev_addr, sizeof(uint32_t));
            break;
        case MIB_RX2_DR:
            mlme_confirm->status = GNRC_LORAWAN_REQ_STATUS_SUCCESS;
            gnrc_lorawan_set_rx2_dr(mac, mlme_request->mib.rx2_dr);
            break;
        default:
            break;
    }
}

static void _mlme_get(gnrc_lorawan_t *mac, const mlme_request_t *mlme_request,
                               mlme_confirm_t *mlme_confirm)
{
    switch(mlme_request->mib.type) {
        case MIB_ACTIVATION_METHOD:
            mlme_confirm->status = GNRC_LORAWAN_REQ_STATUS_SUCCESS;
            mlme_confirm->mib.activation = mac->mlme.activation;
            break;
        case MIB_DEV_ADDR:
            mlme_confirm->status = GNRC_LORAWAN_REQ_STATUS_SUCCESS;
            mlme_confirm->mib.dev_addr = &mac->dev_addr;
            break;
        default:
            mlme_confirm->status = -EINVAL;
            break;
    }
}

void gnrc_lorawan_mlme_request(gnrc_lorawan_t *mac, const mlme_request_t *mlme_request,
                               mlme_confirm_t *mlme_confirm)
{
    switch (mlme_request->type) {
        case MLME_JOIN:
            if(mac->mlme.activation != MLME_ACTIVATION_NONE) {
                mlme_confirm->status = -EINVAL;
                return;
            }
            if (!gnrc_lorawan_mac_acquire(mac)) {
                mlme_confirm->status = -EBUSY;
                return;
            }

            if (mac->mlme.backoff_budget < 0) {
                mlme_confirm->status = -EDQUOT;
                break;
            }
            memcpy(mac->appskey, mlme_request->join.appkey, LORAMAC_APPKEY_LEN);
            mlme_confirm->status = gnrc_lorawan_send_join_request(mac, mlme_request->join.deveui,
                                                                  mlme_request->join.appeui, mlme_request->join.appkey, mlme_request->join.dr);
            break;
        case MLME_LINK_CHECK:
            mac->mlme.pending_mlme_opts |= GNRC_LORAWAN_MLME_OPTS_LINK_CHECK_REQ;
            mlme_confirm->status = GNRC_LORAWAN_REQ_STATUS_DEFERRED;
            break;
        case MLME_SET:
            _mlme_set(mac, mlme_request, mlme_confirm);
            break;
        case MLME_GET:
            _mlme_get(mac, mlme_request, mlme_confirm);
            break;
        case MLME_RESET:
            gnrc_lorawan_reset(mac);
            mlme_confirm->status = GNRC_LORAWAN_REQ_STATUS_SUCCESS;
            break;
        default:
            break;
    }
}

int _fopts_mlme_link_check_req(lorawan_buffer_t *buf)
{
    if (buf) {
        assert(buf->index + GNRC_LORAWAN_CID_SIZE <= buf->size);
        buf->data[buf->index++] = GNRC_LORAWAN_CID_LINK_CHECK_REQ_ANS;
    }

    return GNRC_LORAWAN_CID_SIZE;
}

static int _mlme_link_check_ans(gnrc_lorawan_t *mac, lorawan_buffer_t *fopt)
{
    if (fopt->index + GNRC_LORAWAN_FOPT_LINK_ANS_SIZE > fopt->size) {
        return -EINVAL;
    }
    fopt->index++;

    mlme_confirm_t mlme_confirm;
    mlme_confirm.link_req.margin = fopt->data[fopt->index++];
    mlme_confirm.link_req.num_gateways = fopt->data[fopt->index++];

    mlme_confirm.type = MLME_LINK_CHECK;
    mlme_confirm.status = GNRC_LORAWAN_REQ_STATUS_SUCCESS;
    gnrc_lorawan_mlme_confirm(mac, &mlme_confirm);

    mac->mlme.pending_mlme_opts &= ~GNRC_LORAWAN_MLME_OPTS_LINK_CHECK_REQ;

    return 0;
}

void gnrc_lorawan_process_fopts(gnrc_lorawan_t *mac, uint8_t *fopts, size_t size)
{
    if (!fopts || !size) {
        return;
    }

    lorawan_buffer_t buf;
    _buffer_reset(&buf, fopts, size);

    while (buf.index < buf.size) {
        switch (*(buf.data)) {
            case GNRC_LORAWAN_CID_LINK_CHECK_REQ_ANS:
                if (_mlme_link_check_ans(mac, &buf) < 0) {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

uint8_t gnrc_lorawan_build_options(gnrc_lorawan_t *mac, lorawan_buffer_t *buf)
{
    size_t size = 0;

    size += (mac->mlme.pending_mlme_opts & GNRC_LORAWAN_MLME_OPTS_LINK_CHECK_REQ) ?
            _fopts_mlme_link_check_req(buf) : 0;
    return size;
}

void gnrc_lorawan_mlme_no_rx(gnrc_lorawan_t *mac)
{
    mlme_confirm_t mlme_confirm;

    mlme_confirm.status = -ETIMEDOUT;

    if (mac->mlme.activation == MLME_ACTIVATION_NONE) {
        mlme_confirm.type = MLME_JOIN;
        gnrc_lorawan_mlme_confirm(mac, &mlme_confirm);
    }
    else if (mac->mlme.pending_mlme_opts & GNRC_LORAWAN_MLME_OPTS_LINK_CHECK_REQ) {
        mlme_confirm.type = MLME_LINK_CHECK;
        gnrc_lorawan_mlme_confirm(mac, &mlme_confirm);
        mac->mlme.pending_mlme_opts &= ~GNRC_LORAWAN_MLME_OPTS_LINK_CHECK_REQ;
    }
}

/** @} */
