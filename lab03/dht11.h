/*!
 * \file      dht11.h
 * \brief     API for reading temperature and humidity from a DHT11 sensor.
 * \copyright ARM University Program &copy; ARM Ltd 2014.
 */
#ifndef DHT11_H
#define DHT11_H

#include <stdint.h>

/*! \brief Reads 5 bytes of data from a DHT11 sensor.
 *  \param[out] data_out  Pointer to a 5-byte buffer:
 *                        [0] humidity integer part,
 *                        [1] humidity decimal (always 0 for DHT11),
 *                        [2] temperature integer part,
 *                        [3] temperature decimal (always 0 for DHT11),
 *                        [4] checksum.
 *  \return 0 on success (checksum valid), -1 on failure.
 */
uint8_t* dht11_poll();

#endif // DHT11_H
