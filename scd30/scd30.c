/*
 * Copyright (c) 2018, Sensirion AG
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * * Neither the name of Sensirion AG nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "scd30.h"
#include "scd_git_version.h"
#include "sensirion_arch_config.h"
#include "sensirion_common.h"
#include "sensirion_i2c.h"

#ifdef SCD_ADDRESS
static const uint8_t SCD_I2C_ADDRESS = SCD_ADDRESS;
#else
static const uint8_t SCD_I2C_ADDRESS = 0x61;
#endif

#define SCD_CMD_START_PERIODIC_MEASUREMENT 0x0010
#define SCD_CMD_STOP_PERIODIC_MEASUREMENT 0x0104
#define SCD_CMD_READ_MEASUREMENT 0x0300
#define SCD_CMD_SET_MEASUREMENT_INTERVAL 0x4600
#define SCD_CMD_GET_DATA_READY 0x0202
#define SCD_CMD_SET_TEMPERATURE_OFFSET 0x5403
#define SCD_CMD_SET_ALTITUDE 0x5102
#define SCD_CMD_SET_FORCED_RECALIBRATION 0x5204
#define SCD_CMD_AUTO_SELF_CALIBRATION 0x5306
#define SCD_WRITE_DELAY_US 20000

#define SCD_MAX_BUFFER_WORDS 24
#define SCD_CMD_SINGLE_WORD_BUF_LEN                                            \
    (SENSIRION_COMMAND_SIZE + SENSIRION_WORD_SIZE + CRC8_LEN)

int16_t scd_start_periodic_measurement(uint16_t ambient_pressure_mbar) {
    if (ambient_pressure_mbar &&
        (ambient_pressure_mbar < 700 || ambient_pressure_mbar > 1400)) {
        /* out of allowable range */
        return STATUS_FAIL;
    }

    return sensirion_i2c_write_cmd_with_args(
        SCD_I2C_ADDRESS, SCD_CMD_START_PERIODIC_MEASUREMENT,
        &ambient_pressure_mbar, SENSIRION_NUM_WORDS(ambient_pressure_mbar));
}

int16_t scd_stop_periodic_measurement() {
    return sensirion_i2c_write_cmd(SCD_I2C_ADDRESS,
                                   SCD_CMD_STOP_PERIODIC_MEASUREMENT);
}

int16_t scd_read_measurement(float32_t *co2_ppm, float32_t *temperature,
                             float32_t *humidity) {
    int16_t ret;
    union {
        uint32_t u32_value;
        float32_t float32;
        uint16_t words[2];
    } tmp, data[3];

    ret = sensirion_i2c_read_cmd(SCD_I2C_ADDRESS, SCD_CMD_READ_MEASUREMENT,
                                 data->words, SENSIRION_NUM_WORDS(data));
    if (ret != STATUS_OK)
        return ret;

    /* Revert to be16 for be32 conversion below */
    SENSIRION_WORDS_TO_BYTES(data->words, SENSIRION_NUM_WORDS(data));

    tmp.u32_value = be32_to_cpu(data[0].u32_value);
    *co2_ppm = tmp.float32;

    tmp.u32_value = be32_to_cpu(data[1].u32_value);
    *temperature = tmp.float32;

    tmp.u32_value = be32_to_cpu(data[2].u32_value);
    *humidity = tmp.float32;

    return STATUS_OK;
}

int16_t scd_set_measurement_interval(uint16_t interval_sec) {
    int16_t ret;

    if (interval_sec < 2 || interval_sec > 1800) {
        /* out of allowable range */
        return STATUS_FAIL;
    }

    ret = sensirion_i2c_write_cmd_with_args(
        SCD_I2C_ADDRESS, SCD_CMD_SET_MEASUREMENT_INTERVAL, &interval_sec,
        SENSIRION_NUM_WORDS(interval_sec));
    sensirion_sleep_usec(SCD_WRITE_DELAY_US);

    return ret;
}

int16_t scd_get_data_ready(uint16_t *data_ready) {
    return sensirion_i2c_read_cmd(SCD_I2C_ADDRESS, SCD_CMD_GET_DATA_READY,
                                  data_ready, SENSIRION_NUM_WORDS(*data_ready));
}

int16_t scd_set_temperature_offset(uint16_t temperature_offset) {
    int16_t ret;

    ret = sensirion_i2c_write_cmd_with_args(
        SCD_I2C_ADDRESS, SCD_CMD_SET_TEMPERATURE_OFFSET, &temperature_offset,
        SENSIRION_NUM_WORDS(temperature_offset));
    sensirion_sleep_usec(SCD_WRITE_DELAY_US);

    return ret;
}

int16_t scd_set_altitude(uint16_t altitude) {
    int16_t ret;

    ret = sensirion_i2c_write_cmd_with_args(SCD_I2C_ADDRESS,
                                            SCD_CMD_SET_ALTITUDE, &altitude,
                                            SENSIRION_NUM_WORDS(altitude));
    sensirion_sleep_usec(SCD_WRITE_DELAY_US);

    return ret;
}

int16_t scd_get_automatic_self_calibration(uint8_t *asc_enabled) {
    uint16_t word;
    int16_t ret;

    ret = sensirion_i2c_read_cmd(SCD_I2C_ADDRESS, SCD_CMD_AUTO_SELF_CALIBRATION,
                                 &word, SENSIRION_NUM_WORDS(word));
    if (ret != STATUS_OK)
        return ret;

    *asc_enabled = (uint8_t)word;

    return STATUS_OK;
}

int16_t scd_enable_automatic_self_calibration(uint8_t enable_asc) {
    int16_t ret;
    uint16_t asc = !!enable_asc;

    ret = sensirion_i2c_write_cmd_with_args(SCD_I2C_ADDRESS,
                                            SCD_CMD_AUTO_SELF_CALIBRATION, &asc,
                                            SENSIRION_NUM_WORDS(asc));
    sensirion_sleep_usec(SCD_WRITE_DELAY_US);

    return ret;
}

int16_t scd_set_forced_recalibration(uint16_t co2_ppm) {
    int16_t ret;

    ret = sensirion_i2c_write_cmd_with_args(
        SCD_I2C_ADDRESS, SCD_CMD_SET_FORCED_RECALIBRATION, &co2_ppm,
        SENSIRION_NUM_WORDS(co2_ppm));
    sensirion_sleep_usec(SCD_WRITE_DELAY_US);

    return ret;
}

const char *scd_get_driver_version() {
    return SCD_DRV_VERSION_STR;
}

uint8_t scd_get_configured_address() {
    return SCD_I2C_ADDRESS;
}

int16_t scd_probe() {
    uint16_t data_ready;

    /* Initialize I2C */
    sensirion_i2c_init();

    /* try to read data-ready state */
    return scd_get_data_ready(&data_ready);
}
