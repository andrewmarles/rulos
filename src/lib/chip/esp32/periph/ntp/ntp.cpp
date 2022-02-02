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
  if (_req_time_usec != 0) {
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
  _req_time_usec = wallclock_get_uptime_usec(&_uptime);
  if (sent < 0) {
    LOG("[NTP] could not send UDP packet to %s: errno %d", _hostname, errno);
    _req_time_usec = 0;
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
  uint64_t _resp_time_usec = wallclock_get_uptime_usec(&_uptime);
  int len = recvfrom(_sock, &resp, sizeof(resp), MSG_DONTWAIT, NULL, NULL);
  uint32_t rtt_usec = _resp_time_usec - _req_time_usec;

  if (len == -1) {
    // no data available yet
    if (errno == EWOULDBLOCK) {
      // how long have we been waiting for a response?
      if (rtt_usec > NTP_TIMEOUT_US) {
        LOG("[NTP] Timeout waiting for response");
        _req_time_usec = 0;
        return;
      } else {
        schedule_us(0, NtpClient::_try_receive_trampoline, this);
        return;
      }
    }

    // hard error on reception
    LOG("[NTP] could not receive response: %d", errno);
    _req_time_usec = 0;
    return;
  }

  // packet successfully received!
  LOG("[NTP] got response after %u usec", rtt_usec);

  // time at which the ntp server transmitted its response
  uint64_t server_ntp_usec = (((uint64_t)1000000) * ntohl(resp.txTime_frac)) >> 32;
  server_ntp_usec += (uint64_t)1000000 * ntohl(resp.txTime_sec);

  // how long the remote server took between its incoming and outgoing
  // packets
  uint64_t rx_at_server_ntp_usec =
      (((uint64_t)1000000) * ntohl(resp.rxTime_frac)) >> 32;
  rx_at_server_ntp_usec += (uint64_t)1000000 * ntohl(resp.rxTime_sec);
  uint64_t server_delay_usec = server_ntp_usec - rx_at_server_ntp_usec;

  // estimate one way latency: half of (locally observed RTT - server delay)
  uint32_t oneway_latency_usec = (rtt_usec - server_delay_usec) / 2;

  // compute the epoch time
  uint64_t server_epoch_usec =
      server_ntp_usec - ((uint64_t)1000000 * UNIX_EPOCH_NTP_TIMESTAMP);

  // our estimate of the true epoch time when this packet was received locally
  uint64_t epoch_time_when_received = server_epoch_usec + oneway_latency_usec;

  offset_usec = epoch_time_when_received - _resp_time_usec;

  char logbuf[300];
  int loglen = snprintf(
      logbuf, sizeof(logbuf),
      "[NTP] stats: local_rx_time=%llu,server_delay=%llu,server_epoch_usec=%llu,"
      "raw_rtt_usec=%u,onway_rtt_usec=%u,offset_usec=%lld,",
      _resp_time_usec, server_delay_usec, server_epoch_usec, rtt_usec, oneway_latency_usec,
      offset_usec);

  //_add_observation(local_time_when_sent, server_epoch_usec, logbuf[loglen], sizeof(logbuf)-loglen);
  logbuf[loglen++] = '\n';
  log_write(logbuf, loglen);
  _req_time_usec = 0;
}

void NtpClient::_try_receive_trampoline(void *data) {
  NtpClient *ntp = static_cast<NtpClient *>(data);
  ntp->_try_receive();
}

bool NtpClient::is_synced(void) {
  return (offset_usec != 0);
}

uint32_t NtpClient::get_epoch_time_sec(void) {
  return get_epoch_time_usec() / 1000000;
}

uint64_t NtpClient::get_epoch_time_usec(void) {
  if (is_synced()) {
    return wallclock_get_uptime_usec(&_uptime) + offset_usec;
  } else {
    return 0;
  }
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
