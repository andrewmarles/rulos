/*
 * Copyright (C) 2009 Jon Howell (jonh@jonh.net) and Jeremy Elson
 * (jelson@gmail.com).
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "periph/ntp/ntp.h"

#include <stdio.h>
#include <stdlib.h>

#include "core/rulos.h"
#include "core/wallclock.h"
#include "periph/ntp/ntp-packet.h"

extern "C" {
#include <errno.h>
#include <lwip/netdb.h>
#include <lwip/sockets.h>
}

bool NtpClient::_sendRequest(ntp_packet_t *req) {
  // ensure there isn't already a transaction outstanding
  if (_req_time != 0) {
    LOG("[NTP] not sending req; one is outstanding already");
    return false;
  }

  // resolve hostname
  struct hostent *server;
  server = gethostbyname(_hostname);
  if (server == NULL) {
    LOG("[NTP] could not resolve '%s': errno %d", _hostname, errno);
    return false;
  }
  uint32_t remote_ip;
  if (server->h_addrtype != AF_INET) {
    LOG("[NTP] got weird addr type of %d for host %s", server->h_addrtype,
        _hostname);
    return false;
  }

  // log and set destination address
  memcpy(&remote_ip, server->h_addr_list[0], sizeof(remote_ip));
  struct in_addr a;
  a.s_addr = remote_ip;
  LOG("[NTP] resolved %s to %s", _hostname, inet_ntoa(a));
  struct sockaddr_in dest;
  dest.sin_addr.s_addr = remote_ip;
  dest.sin_family = AF_INET;
  dest.sin_port = htons(NTP_PORT);

  // create socket, if not already created
  if (_sock == -1) {
    if ((_sock = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
      LOG("[NTP] could not create socket: %d", errno);
      return false;
    }

    fcntl(_sock, F_SETFL, O_NONBLOCK);
  }

  int sent = sendto(_sock, req, sizeof(*req), 0, (struct sockaddr *)&dest,
                    sizeof(dest));
  _req_time = wallclock_get_uptime_usec(&_uptime);
  if (sent < 0) {
    LOG("[NTP] could not send UDP packet to %s: errno %d", _hostname, errno);
    _req_time = 0;
    return false;
  }

  LOG("[NTP] sent request");
  return true;
}

static int num_req = 0;
void NtpClient::_schedule_next_sync() {
  num_req++;
  if (num_req < 50) {
    schedule_us(15000000, NtpClient::_sync_trampoline, this);
  }
}

void NtpClient::_sync() {
  _schedule_next_sync();

  ntp_packet_t req;
  memset(&req, 0, sizeof(req));
  req.version = 3;
  req.mode = 3;

  bool ok = _sendRequest(&req);
  if (!ok) {
    LOG("[NTP] request failed");
    return;
  }

  // request succeeded! schedule an attempt at reception
  schedule_us(0, NtpClient::_try_receive_trampoline, this);
}

void NtpClient::_sync_trampoline(void *data) {
  NtpClient *ntp = static_cast<NtpClient *>(data);
  ntp->_sync();
}

void NtpClient::_try_receive() {
  ntp_packet_t resp;
  Time _resp_time = wallclock_get_uptime_usec(&_uptime);
  int len = recvfrom(_sock, &resp, sizeof(resp), MSG_DONTWAIT, NULL, NULL);
  Time rtt = _resp_time - _req_time;

  if (len == -1) {
    // no data available yet
    if (errno == EWOULDBLOCK) {
      // how long have we been waiting for a response?
      if (rtt > NTP_TIMEOUT_US) {
        LOG("[NTP] Timeout waiting for response");
        _req_time = 0;
        return;
      } else {
        schedule_us(0, NtpClient::_try_receive_trampoline, this);
        return;
      }
    }

    // hard error on reception
    LOG("[NTP] could not receive response: %d", errno);
    _req_time = 0;
    return;
  }

  // packet successfully received!
  LOG("[NTP] got response after %u uesc", rtt);

  // compute the time at which the response packet was transmitted by
  // the server
  uint64_t ntp_usecs = (((uint64_t) 1000000) * ntohl(resp.txTime_frac)) >> 32;
  ntp_usecs += (uint64_t) 1000000 * ntohl(resp.txTime_sec);

  // compute how long the server took between incoming and outgoing packets
  uint64_t rx_at_server_ntp_usecs = (((uint64_t) 1000000) * ntohl(resp.rxTime_frac)) >> 32;
  rx_at_server_ntp_usecs += (uint64_t) 1000000 * ntohl(resp.rxTime_sec);
  uint64_t server_delay_usec = ntp_usecs - rx_at_server_ntp_usecs;

  uint64_t epoch_usecs = ntp_usecs - ((uint64_t) 1000000 * UNIX_EPOCH_NTP_TIMESTAMP);
  int64_t offset_usecs = epoch_usecs - _resp_time;
  LOG("[NTP] stats: local_rx_time=%d, server_delay=%llu, epoch_usec=%llu, raw_rtt_usec=%u, offset_usec=%lld",
      _resp_time, server_delay_usec,
      epoch_usecs, rtt, offset_usecs);
  _req_time = 0;
}

void NtpClient::_try_receive_trampoline(void *data) {
  NtpClient *ntp = static_cast<NtpClient *>(data);
  ntp->_try_receive();
}

bool NtpClient::is_synced(void) {
  return false;
}

uint32_t NtpClient::get_epoch_time_sec(void) {
  return 0;
}

uint64_t NtpClient::get_epoch_time_msec(void) {
  return 0;
}

void NtpClient::start(void) {
  wallclock_init(&_uptime);
  schedule_now(NtpClient::_sync_trampoline, this);
}

void NtpClient::_init(const char *hostname) {
  _sock = -1;
  _hostname = hostname;
}

NtpClient::NtpClient() {
  _init(DEFAULT_SERVER);
}

NtpClient::NtpClient(const char *hostname) {
  _init(hostname);
}
