/**************************************************************************/
/*!
    @file     adafruit_tcp.cpp
    @author   ktown

    @section LICENSE

    Software License Agreement (BSD License)

    Copyright (c) 2016, Adafruit Industries (adafruit.com)
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:
    1. Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    3. Neither the name of the copyright holders nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ''AS IS'' AND ANY
    EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
/**************************************************************************/

#include "adafruit_wifi.h"
#include "adafruit_tcp.h"

/******************************************************************************/
/*!
    @brief Instantiates a new instance of the AdafruitTCP class
*/
/******************************************************************************/
AdafruitTCP::AdafruitTCP(void)
{
  rx_callback   = NULL;
  this->reset();
}

/******************************************************************************/
/*!
    @brief
*/
/******************************************************************************/
void AdafruitTCP::reset()
{
  _tcp_handle       = 0;
  _bytesRead        = 0;
  _packet_buffering = false;
}

void AdafruitTCP::usePacketBuffering(bool enable)
{
  _packet_buffering = enable;
}

/******************************************************************************/
/*!
    @brief
*/
/******************************************************************************/
int AdafruitTCP::connect(IPAddress ip, uint16_t port)
{
  if ( !feather.connected() ) return 0;

  uint32_t ipv4 = (uint32_t) ip;
  uint8_t is_tls = 0;

  sdep_cmd_para_t para_arr[] =
  {
      { .len = 4, .p_value = &ipv4     },
      { .len = 2, .p_value = &port     },
      { .len = 1, .p_value = &is_tls   },
      { .len = 4, .p_value = &_timeout },
  };

  VERIFY( sdep_n(SDEP_CMD_TCP_CONNECT,
                 sizeof(para_arr)/sizeof(sdep_cmd_para_t), para_arr,
                 NULL, &_tcp_handle), false);

  this->install_callback();

  return true;
}

/******************************************************************************/
/*!
    @brief
*/
/******************************************************************************/
int AdafruitTCP::connect(const char* host, uint16_t port)
{
  IPAddress ip;
  VERIFY( feather.hostByName(host, ip), false );
  return this->connect(ip, port);
}

/******************************************************************************/
/*!
    @brief
*/
/******************************************************************************/
int AdafruitTCP::connectSSL(IPAddress ip, uint16_t port)
{
  uint32_t ipv4 = (uint32_t) ip;
  uint8_t is_tls = 1;

//  uint8_t namelen = (common_name ? strlen(common_name) : 0);

  sdep_cmd_para_t para_arr[] =
  {
      { .len = 4 , .p_value = &ipv4       },
      { .len = 2 , .p_value = &port       },
      { .len = 1 , .p_value = &is_tls     },
      { .len = 4 , .p_value = &_timeout   },
      //{ .len = namelen , .p_value = common_name },
  };

  VERIFY( sdep_n(SDEP_CMD_TCP_CONNECT,
                 sizeof(para_arr)/sizeof(sdep_cmd_para_t) /*- (common_name ? 0 : 1)*/, para_arr,
                 NULL, &_tcp_handle), false);

  this->install_callback();
  return true;
}

/******************************************************************************/
/*!
    @brief
*/
/******************************************************************************/
int AdafruitTCP::connectSSL(const char* host, uint16_t port)
{
  IPAddress ip;
  VERIFY( feather.hostByName(host, ip), false );
  return this->connectSSL(ip, port);
}

/******************************************************************************/
/*!
    @brief
*/
/******************************************************************************/
uint8_t AdafruitTCP::connected()
{
  // Handle not zero --> still connected
  return ( _tcp_handle != 0 ) ? 1 : 0;
}

/******************************************************************************/
/*!
    @brief  Reads a single byte from the response stream

    @return Returns -1 if no data was available, or the total number of bytes
            read so far if a byte was able to be read.
*/
/******************************************************************************/
int AdafruitTCP::read()
{
  uint8_t b;
  return ( this->read(&b, 1) > 0 ) ? b : EOF;
}

/******************************************************************************/
/*!
    @brief  Reads the specified number of bytes, placing them in 'buf'

    @return The total number of bytes read so far
*/
/******************************************************************************/
int AdafruitTCP::read(uint8_t* buf, size_t size)
{
  if ( _tcp_handle == 0 ) return 0;

  uint16_t size16 = (uint16_t) size;

  sdep_cmd_para_t para_arr[] =
  {
      { .len = 4, .p_value = &_tcp_handle },
      { .len = 2, .p_value = &size16      },
      { .len = 4, .p_value = &_timeout     },
  };

  // TODO check case when read bytes < size
  VERIFY( sdep_n(SDEP_CMD_TCP_READ,
                 sizeof(para_arr)/sizeof(sdep_cmd_para_t),
                 para_arr, &size16, buf), 0);

  _bytesRead += size;
  return size;
}

/******************************************************************************/
/*!
    @brief  Writes a byte
*/
/******************************************************************************/
size_t AdafruitTCP::write( uint8_t b)
{
  return write(&b, 1);
}

/******************************************************************************/
/*!
    @brief  Writes the specified number of bytes
*/
/******************************************************************************/
size_t AdafruitTCP::write(const uint8_t* content, size_t len)
{
  if (_tcp_handle == 0) return 0;

  sdep_cmd_para_t para_arr[] =
  {
      { .len = 4  , .p_value = &_tcp_handle},
      { .len = len, .p_value = content    }
  };

  VERIFY( sdep_n(SDEP_CMD_TCP_WRITE,
                 sizeof(para_arr)/sizeof(sdep_cmd_para_t), para_arr,
                 NULL, NULL), 0);

  // if packet is not buffered --> send out immediately
  if (!_packet_buffering) this->flush();

  return len;
}

/******************************************************************************/
/*!
    @brief
*/
/******************************************************************************/
void AdafruitTCP::flush()
{
  if ( _tcp_handle == 0 ) return;

  // flush write
  sdep(SDEP_CMD_TCP_FLUSH, 4, &_tcp_handle, NULL, NULL);

  // TODO should we flush read bytes as well ??
  //while (available()) read();
}

/******************************************************************************/
/*!
    @brief  Returns the number of bytes available in the response FIFO
*/
/******************************************************************************/
int AdafruitTCP::available()
{
  if ( _tcp_handle == 0 ) return 0;

  uint8_t result = 0;
  sdep(SDEP_CMD_TCP_AVAILABLE, 4, &_tcp_handle, NULL, &result);

  return result;
}

/******************************************************************************/
/*!
    @brief
*/
/******************************************************************************/
int AdafruitTCP::peek()
{
  if ( _tcp_handle == 0 ) return EOF;

  sdep_cmd_para_t para_arr[] =
  {
      { .len = 4, .p_value = &_tcp_handle },
      { .len = 4, .p_value = &_timeout    },
  };

  char ch = EOF;
  sdep_n(SDEP_CMD_TCP_PEEK,
         sizeof(para_arr)/sizeof(sdep_cmd_para_t), para_arr,
         NULL, &ch);

  return (int) ch;
}

/******************************************************************************/
/*!
    @brief  Passes the callback references to the lower WiFi layer
*/
/******************************************************************************/
void AdafruitTCP::install_callback(void)
{
  if (disconnect_callback == NULL && rx_callback == NULL) return;

  sdep_cmd_para_t para_arr[] =
  {
      { .len = 4, .p_value = &_tcp_handle         },
      { .len = 4, .p_value = &disconnect_callback },
      { .len = 4, .p_value = &rx_callback         },
  };

  // TODO check case when read bytes < size
  sdep_n(SDEP_CMD_TCP_SET_CALLBACK,
         sizeof(para_arr)/sizeof(sdep_cmd_para_t), para_arr,
         NULL, NULL);
}

/******************************************************************************/
/*!
    @brief  Sets the data received callback for the user code
*/
/******************************************************************************/
void AdafruitTCP::setReceivedCallback( int (*fp) (void*, void*) )
{
  rx_callback = fp;
}

/******************************************************************************/
/*!
    @brief  Sets the disconnect callback for the user code
*/
/******************************************************************************/
void AdafruitTCP::setDisconnectCallback( int (*fp) (void*, void*))
{
  disconnect_callback = fp;
}

/******************************************************************************/
/*!
    @brief  Closes the TCP socket and resets any counters
*/
/******************************************************************************/
void AdafruitTCP::stop()
{
  if ( _tcp_handle == 0 ) return;

  sdep(SDEP_CMD_TCP_CLOSE, 4, &_tcp_handle, NULL, NULL);
  this->reset();
}
