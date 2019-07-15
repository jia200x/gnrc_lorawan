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
 * @author  Francisco Molina <femolina@uc.cl>
 */
#include <stdio.h>
#include <string.h>

#include "gnrc_lorawan/lorawan.h"
#include "gnrc_lorawan_internal.h"
#include "byteorder.h"
#include "net/lorawan/hdr.h"

#define MIC_B0_START (0x49)
#define CRYPT_B0_START (0x01)
#define DIR_MASK (0x1)
#define SBIT_MASK (0xF)

#define APP_SKEY_B0_START (0x1)
#define NWK_SKEY_B0_START (0x2)

static uint8_t digest[LORAMAC_APPKEY_LEN];

typedef struct  __attribute__((packed)) {
    uint8_t fb;
    uint32_t u8_pad;
    uint8_t dir;
    le_uint32_t dev_addr;
    le_uint32_t fcnt;
    uint8_t u32_pad;
    uint8_t len;
} lorawan_block_t;

void gnrc_lorawan_calculate_join_mic(gnrc_lorawan_t *mac, const uint8_t *buf, size_t len, const uint8_t *key, le_uint32_t *out)
{
    gnrc_lorawan_cmac_init(mac, key);
    gnrc_lorawan_cmac_update(mac, buf, len);
    gnrc_lorawan_cmac_finish(mac, digest);

    memcpy(out, digest, sizeof(le_uint32_t));
}

void gnrc_lorawan_calculate_mic(gnrc_lorawan_t *mac, const le_uint32_t *dev_addr, uint32_t fcnt,
                                uint8_t dir, uint8_t *buf, size_t len, const uint8_t *nwkskey, le_uint32_t *out)
{
    lorawan_block_t block;

    block.fb = MIC_B0_START;
    block.u8_pad = 0;
    block.dir = dir & DIR_MASK;

    memcpy(&block.dev_addr, dev_addr, sizeof(le_uint32_t));

    block.fcnt = byteorder_btoll(byteorder_htonl(fcnt));

    block.u32_pad = 0;

    block.len = len;

    gnrc_lorawan_cmac_init(mac, nwkskey);
    gnrc_lorawan_cmac_update(mac, &block, sizeof(block));
    gnrc_lorawan_cmac_update(mac, buf, len);
    gnrc_lorawan_cmac_finish(mac, digest);

    memcpy(out, digest, sizeof(le_uint32_t));
}

void gnrc_lorawan_encrypt_payload(gnrc_lorawan_t *mac, uint8_t *buf, size_t len, const le_uint32_t *dev_addr, uint32_t fcnt, uint8_t dir, const uint8_t *appskey)
{
    uint8_t s_block[16];
    uint8_t a_block[16];

    memset(s_block, 0, sizeof(s_block));
    memset(a_block, 0, sizeof(a_block));

    lorawan_block_t *block = (lorawan_block_t *) a_block;

    gnrc_lorawan_aes128_init(mac, appskey);

    block->fb = CRYPT_B0_START;

    block->u8_pad = 0;
    block->dir = dir & DIR_MASK;

    block->dev_addr = *dev_addr;
    block->fcnt = byteorder_btoll(byteorder_htonl(fcnt));

    block->u32_pad = 0;

    int c = 0;
    for (unsigned i = 0; i < len; i++) {
        if ((c & SBIT_MASK) == 0) {
            block->len = (c >> 4) + 1;
            gnrc_lorawan_aes128_encrypt(mac, a_block, s_block);
        }

        buf[i] = buf[i] ^ s_block[c & SBIT_MASK];
        c++;
    }
}

void gnrc_lorawan_decrypt_join_accept(gnrc_lorawan_t *mac, const uint8_t *key, uint8_t *pkt, int has_clist, uint8_t *out)
{
    gnrc_lorawan_aes128_init(mac, key);
    gnrc_lorawan_aes128_encrypt(mac, pkt, out);

    if (has_clist) {
        gnrc_lorawan_aes128_encrypt(mac, pkt + LORAMAC_APPKEY_LEN, out + LORAMAC_APPKEY_LEN);
    }
}

void gnrc_lorawan_generate_session_keys(gnrc_lorawan_t *mac, const uint8_t *app_nonce, const uint8_t *dev_nonce, const uint8_t *appkey, uint8_t *nwkskey, uint8_t *appskey)
{
    uint8_t buf[LORAMAC_APPSKEY_LEN];

    memset(buf, 0, sizeof(buf));

    gnrc_lorawan_aes128_init(mac, appkey);

    /* net_id comes right after app_nonce */
    memcpy(buf + 1, app_nonce, GNRC_LORAWAN_APP_NONCE_SIZE + GNRC_LORAWAN_NET_ID_SIZE);
    memcpy(buf + 1 + GNRC_LORAWAN_APP_NONCE_SIZE + GNRC_LORAWAN_NET_ID_SIZE, dev_nonce, GNRC_LORAWAN_DEV_NONCE_SIZE);

    /* Calculate Application Session Key */
    buf[0] = APP_SKEY_B0_START;
    gnrc_lorawan_aes128_encrypt(mac, buf, nwkskey);

    /* Calculate Network Session Key */
    buf[0] = NWK_SKEY_B0_START;
    gnrc_lorawan_aes128_encrypt(mac, buf, appskey);
}

/** @} */
