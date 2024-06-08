/*
 * server_test.c
 *
 *  Created on: Jul 19, 2017
 *      Author: admin
 */
#include "lwip.h"

#define echo_server_dbg(x) (void) 0

static void echo_server_recv(void *arg, struct udp_pcb *upcb, struct pbuf *p,
                             const ip_addr_t *addr, u16_t port);
static void echo_server_err(void *arg, err_t err);
static err_t echo_server_sent(void *arg, struct tcp_pcb *pcb, u16_t len);
static int echo_server_send(struct tcp_pcb *pcb, const char *buf, size_t len);

// Function to handle incoming UDP packets
static void echo_server_recv(void *arg, struct udp_pcb *upcb, struct pbuf *p,
                             const ip_addr_t *addr, u16_t port) {
    printf("echo_server_recv\n");
    if (p != NULL) {

        printf("recv UDP: %s \n", p);
        // Echo the received data back to the sender
        udp_sendto(upcb, p, addr, port);

        // Free the pbuf
        pbuf_free(p);
    }
}

// Function to create and initialize the UDP echo server
err_t create_echo_server(void) {
    struct udp_pcb *upcb;
    err_t err;

    // Create a new UDP PCB
    upcb = udp_new();
    if (upcb == NULL) {
        return ERR_MEM;
    }

    // Bind the UDP PCB to any IP address and the specified port
    err = udp_bind(upcb, IP_ADDR_ANY, TCP_LOCAL_SERVER_PORT);
    if (err != ERR_OK) {
        udp_remove(upcb);
        return err;
    }

    printf("Echo server started at port %d\n", TCP_LOCAL_SERVER_PORT);

    // Set up the receive callback function
    udp_recv(upcb, echo_server_recv, NULL);

    return ERR_OK;
}

static void echo_server_err(void *arg, err_t err)
{
  LWIP_UNUSED_ARG(err);
  echo_server_dbg(("echo_server_err: %s", lwip_strerr(err)));
}

static err_t echo_server_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
  echo_server_dbg(("echo_server_sent %p\n", (void*)pcb));
  LWIP_UNUSED_ARG(len);
  return ERR_OK;
}

static int echo_server_send(struct tcp_pcb *pcb, const char *buf, size_t len)
{
  u16_t max_len;
  err_t err = ERR_OK;
  size_t offset = 0;
  size_t snd_len  = len;
  u8_t apiflags = TCP_WRITE_FLAG_COPY; /* Data is copy to sending buffer: Need to optimize */

  if (pcb == NULL || buf == NULL || len == 0)
  {
	  echo_server_dbg(("Bad input!\n"));
	  return ERR_ARG;
  }

  while(1)
  {
    /* We cannot send more data than space available in the send buffer. */
    max_len = tcp_sndbuf(pcb);
    if (max_len < snd_len)
    {
      snd_len = max_len;
    }
    do {
      /* Write packet to out-queue, but do not send it until tcp_output() is called. */
      err = tcp_write(pcb, buf + offset, snd_len, apiflags);
      if (err == ERR_MEM)
      {
        if ((tcp_sndbuf(pcb) == 0) ||
            (tcp_sndqueuelen(pcb) >= TCP_SND_QUEUELEN)) {
          /* no need to try smaller sizes */
          snd_len = 1;
        } else {
          snd_len /= 2;
        }
      }
      if (err == ERR_OK)
      {
        tcp_output(pcb); /* Send the packet immediately */
      }
    } while ((err == ERR_MEM) && (snd_len > 1));

    if (err == ERR_OK) {
      offset += snd_len;
      snd_len = len - snd_len;
    }
    if (snd_len == 0)
    {
      break;
    }
  }

  /* ensure nagle is normally enabled (only disabled for persistent connections
      when all data has been enqueued but the connection stays open for the next
      request */
  tcp_nagle_enable(pcb);
  return offset;
}
