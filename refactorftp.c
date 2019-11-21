static CURLcode AllowServerConnect(struct connectdata *conn)
{
  struct SessionHandle *data = conn->data;
  curl_socket_t sock = conn->sock[SECONDARYSOCKET];
  long timeout_ms;
  long interval_ms;
  curl_socket_t s = CURL_SOCKET_BAD;
#ifdef ENABLE_IPV6
  struct Curl_sockaddr_storage add;
#else
  struct sockaddr_in add;
#endif
  curl_socklen_t size = (curl_socklen_t) sizeof(add);

  for(;;) {
    timeout_ms = Curl_timeleft(data, NULL, TRUE);

    if(timeout_ms < 0) {
      /* if a timeout was already reached, bail out */
      failf(data, "Timeout while waiting for server connect");
      return CURLE_OPERATION_TIMEDOUT;
    }

    interval_ms = 1000;  /* use 1 second timeout intervals */
    if(timeout_ms < interval_ms)
      interval_ms = timeout_ms;

    switch (Curl_socket_ready(sock, CURL_SOCKET_BAD, (int)interval_ms)) {
    case -1: /* error */
      /* let's die here */
      failf(data, "Error while waiting for server connect");
      return CURLE_FTP_PORT_FAILED;
    case 0:  /* timeout */
      break; /* loop */
    default:
      /* we have received data here */
      if(0 == getsockname(sock, (struct sockaddr *) &add, &size)) {
        size = sizeof(add);

        s=accept(sock, (struct sockaddr *) &add, &size);
      }
      sclose(sock); /* close the first socket */

      if(CURL_SOCKET_BAD == s) {
        failf(data, "Error accept()ing server connect");
        return CURLE_FTP_PORT_FAILED;
      }
      infof(data, "Connection accepted from server\n");

      conn->sock[SECONDARYSOCKET] = s;
      curlx_nonblock(s, TRUE); /* enable non-blocking */
      return CURLE_OK;
    } /* switch() */
  }
  /* never reaches this point */
}

static int ftp_endofresp(struct pingpong *pp,
                         int *code)
{
  char *line = pp->linestart_resp;
  size_t len = pp->nread_resp;

  if((len > 3) && LASTLINE(line)) {
    *code = curlx_sltosi(strtol(line, NULL, 10));
    return 1;
  }
  return 0;
}

static void doloop(int *ftpcode,CURLcode result){
while(!*ftpcode && !result) {
    /* check and reset timeout value every lap */
    timeout = Curl_pp_state_timeout(pp);

    if(timeout <=0 ) {
      failf(data, "FTP response timeout");
      return CURLE_OPERATION_TIMEDOUT; /* already too little time */
    }

    interval_ms = 1000;  /* use 1 second timeout intervals */
    if(timeout < interval_ms)
      interval_ms = timeout;

    /*
     * Since this function is blocking, we need to wait here for input on the
     * connection and only then we call the response reading function. We do
     * timeout at least every second to make the timeout check run.
     *
     * A caution here is that the ftp_readresp() function has a cache that may
     * contain pieces of a response from the previous invoke and we need to
     * make sure we don't just wait for input while there is unhandled data in
     * that cache. But also, if the cache is there, we call ftp_readresp() and
     * the cache wasn't good enough to continue we must not just busy-loop
     * around this function.
     *
     */

    if(pp->cache && (cache_skip < 2)) {
      /*
       * There's a cache left since before. We then skipping the wait for
       * socket action, unless this is the same cache like the previous round
       * as then the cache was deemed not enough to act on and we then need to
       * wait for more data anyway.
       */
    }
    else {
      switch (Curl_socket_ready(sockfd, CURL_SOCKET_BAD, (int)interval_ms)) {
      case -1: /* select() error, stop reading */
        failf(data, "FTP response aborted due to select/poll error: %d",
              SOCKERRNO);
        return CURLE_RECV_ERROR;

      case 0: /* timeout */
        if(Curl_pgrsUpdate(conn))
          return CURLE_ABORTED_BY_CALLBACK;
        continue; /* just continue in our loop for the timeout duration */

      default: /* for clarity */
        break;
      }
    }
    result = ftp_readresp(sockfd, pp, ftpcode, &nread);
    if(result)
      break;

    if(!nread && pp->cache)
      /* bump cache skip counter as on repeated skips we must wait for more
         data */
      cache_skip++;
    else
      /* when we got data or there is no cache left, we reset the cache skip
         counter */
      cache_skip=0;

    *nreadp += nread;

  }
}
