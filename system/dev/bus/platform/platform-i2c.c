// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ddk/debug.h>
#include <ddk/protocol/i2c.h>
#include <zircon/threads.h>
#include <stdlib.h>
#include <threads.h>

#include "platform-bus.h"

typedef struct i2c_driver_channel {
    i2c_driver_protocol_t i2c;
    i2c_channel_t channel;
    uint32_t channel_id;

//    zx_handle_t socket; // socket used for communicating with our client
//    zx_handle_t event;  // event for signaling serial driver state changes
    thrd_t thread;
    mtx_t lock;
} i2c_driver_channel_t;

zx_status_t platform_i2c_init(platform_bus_t* bus, i2c_driver_protocol_t* i2c) {
    if (bus->i2c_channels) {
        // already initialized
        return ZX_ERR_BAD_STATE;
    }

    size_t channel_count = i2c_driver_get_channel_count(i2c);
    if (!channel_count) {
        return ZX_ERR_NOT_SUPPORTED;
    }

    i2c_driver_channel_t* channels = calloc(channel_count, sizeof(i2c_driver_channel_t));
    if (!channels) {
        return ZX_ERR_NO_MEMORY;
    }

    for (uint32_t i = 0; i < channel_count; i++) {
        i2c_driver_channel_t* channel = &channels[i];
        mtx_init(&channel->lock, mtx_plain);
        memcpy(&channel->i2c, i2c, sizeof(channel->i2c));
        channel->channel_id = i;
    }

    bus->i2c_channels = channels;
    bus->i2c_channel_count = channel_count;

    return ZX_OK;
}

zx_status_t platform_i2c_get_channel(platform_bus_t* bus, uint32_t bus_id, uint16_t address,
                                     size_t* out_max_transfer_size) {
    if (bus_id >= bus->i2c_channel_count) {
        return ZX_ERR_NOT_FOUND;
    }
    i2c_driver_channel_t* channel = &bus->i2c_channels[bus_id];

    zx_status_t status = i2c_get_channel_by_address(&bus->i2c, bus_id, address, &channel->channel);
    if (status != ZX_OK) {
        return status;
    }
    status = i2c_get_max_transfer_size(channel, &out_max_transfer_size);
    if (status != ZX_OK) {
        i2c_channel_release(channel);
        free(channel);
        return status;
    }

    return ZX_OK;
}