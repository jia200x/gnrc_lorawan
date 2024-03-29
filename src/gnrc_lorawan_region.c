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
#include "gnrc_lorawan_internal.h"
#include "gnrc_lorawan/region.h"

#define GNRC_LORAWAN_DATARATES_NUMOF 6

static uint8_t dr_sf[GNRC_LORAWAN_DATARATES_NUMOF] = { LORA_SF12, LORA_SF11, LORA_SF10, LORA_SF9, LORA_SF8, LORA_SF7 };
static uint8_t dr_bw[GNRC_LORAWAN_DATARATES_NUMOF] = { LORA_BW_125_KHZ, LORA_BW_125_KHZ, LORA_BW_125_KHZ, LORA_BW_125_KHZ, LORA_BW_125_KHZ, LORA_BW_125_KHZ };

int gnrc_lorawan_set_dr(gnrc_lorawan_t *mac, uint8_t datarate)
{
    if (!gnrc_lorawan_validate_dr(datarate)) {
        return -EINVAL;
    }

    gnrc_lorawan_radio_set_sf(mac, dr_sf[datarate]);
    gnrc_lorawan_radio_set_bw(mac, dr_bw[datarate]);

    return 0;
}

uint8_t gnrc_lorawan_rx1_get_dr_offset(uint8_t dr_up, uint8_t dr_offset)
{
    return (dr_up > dr_offset) ? (dr_up - dr_offset) : 0;
}

static size_t _get_num_used_channels(gnrc_lorawan_t *mac)
{
    size_t count = 0;

    for (unsigned i = 0; i < GNRC_LORAWAN_MAX_CHANNELS; i++) {
        if (mac->channel[i]) {
            count++;
        }
    }
    return count;
}

static uint32_t _get_nth_channel(gnrc_lorawan_t *mac, size_t n)
{
    int i = 0;
    uint32_t channel = 0;

    while (n) {
        if (mac->channel[i]) {
            n--;
            channel = mac->channel[i];
            i++;
        }
    }
    return channel;
}

void gnrc_lorawan_channels_init(gnrc_lorawan_t *mac)
{
    for(unsigned i = 0; i<GNRC_LORAWAN_DEFAULT_CHANNELS_NUMOF; i++) {
        mac->channel[i] = gnrc_lorawan_default_channels[i];
    }

    for (unsigned i = GNRC_LORAWAN_DEFAULT_CHANNELS_NUMOF; i < GNRC_LORAWAN_MAX_CHANNELS; i++) {
        mac->channel[i] = 0;
    }
}

uint32_t gnrc_lorawan_pick_channel(gnrc_lorawan_t *mac)
{
    uint32_t random_number = gnrc_lorawan_random_get(mac);

    return _get_nth_channel(mac, 1 + (random_number % _get_num_used_channels(mac)));
}

void gnrc_lorawan_process_cflist(gnrc_lorawan_t *mac, uint8_t *cflist)
{
    /* TODO: Check CFListType to 0 */
    for (unsigned i = GNRC_LORAWAN_DEFAULT_CHANNELS_NUMOF; i < 8; i++) {
        le_uint32_t cl;
        cl.u32 = 0;
        memcpy(&cl, cflist, GNRC_LORAWAN_CFLIST_ENTRY_SIZE);
        mac->channel[i] = byteorder_ntohl(byteorder_ltobl(cl))*100;
        cflist += GNRC_LORAWAN_CFLIST_ENTRY_SIZE;
    }
}

uint8_t gnrc_lorawan_region_mac_payload_max(uint8_t datarate)
{
    if (datarate < 3) {
        return GNRC_LORAWAN_MAX_PAYLOAD_1;
    }
    else if (datarate == 3) {
        return GNRC_LORAWAN_MAX_PAYLOAD_2;
    }
    else {
        return GNRC_LORAWAN_MAX_PAYLOAD_3;
    }
}

int gnrc_lorawan_validate_dr(uint8_t dr)
{
    if (dr < GNRC_LORAWAN_DATARATES_NUMOF) {
        return true;
    }
    return false;
}

/** @} */
