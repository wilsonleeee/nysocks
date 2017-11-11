#include "mux.h"
#include "KcpuvTest.h"
#include <iostream>

namespace kcpuv_test {
using namespace std;

class MuxTest : public testing::Test {
protected:
  MuxTest(){};
  virtual ~MuxTest(){};
};

TEST_F(MuxTest, mux_encode_and_decode) {
  char *buf = new char[10];

  int cmd = 10;
  int length = 1400;
  unsigned int id = 65535;

  kcpuv__mux_encode(buf, id, cmd, length);

  EXPECT_EQ(buf[0], cmd);
  EXPECT_EQ(buf[1] & 0xFF, (id >> 24) & 0xFF);
  EXPECT_EQ(buf[2] & 0xFF, (id >> 16) & 0xFF);
  EXPECT_EQ(buf[3] & 0xFF, (id >> 8) & 0xFF);
  EXPECT_EQ((buf[4] & 0xFF), (id)&0xFF);
  EXPECT_EQ(buf[5] & 0xFF, (length >> 8) & 0xFF);
  EXPECT_EQ(buf[6] & 0xFF, (length)&0xFF);

  int decoded_cmd;
  int decoded_length;
  unsigned int decoded_id;

  decoded_id = kcpuv__mux_decode(buf, &decoded_cmd, &decoded_length);

  EXPECT_EQ(id, decoded_id);
  EXPECT_EQ(cmd, decoded_cmd);
  EXPECT_EQ(length, decoded_length);

  delete[] buf;
}

static int received_conns = 0;

void p2_on_msg(kcpuv_mux_conn *conn, char *buffer, int length) {
  EXPECT_EQ(length, 4096);
  kcpuv_mux_send(conn, "hello", 5, KCPUV_MUX_CMD_PUSH);

  received_conns += 1;

  if (received_conns == 2) {
    kcpuv_stop_loop();
  }
}

void on_p2_conn(kcpuv_mux_conn *conn) {
  kcpuv_mux_conn_listen(conn, p2_on_msg);
}

void on_data_return(kcpuv_mux_conn *conn, char *buffer, int length) {
  EXPECT_EQ(length, 5);
}

TEST_F(MuxTest, transmission) {
  kcpuv_initialize();

  kcpuv_sess *sess_p1 = kcpuv_create();
  kcpuv_sess *sess_p2 = kcpuv_create();

  kcpuv_listen(sess_p1, 0, NULL);
  kcpuv_listen(sess_p2, 0, NULL);

  char *addr_p1 = new char[16];
  char *addr_p2 = new char[16];
  int namelen_p1;
  int namelen_p2;
  int port_p1;
  int port_p2;

  kcpuv_get_address(sess_p1, addr_p1, &namelen_p1, &port_p1);
  kcpuv_get_address(sess_p2, addr_p2, &namelen_p2, &port_p2);
  kcpuv_init_send(sess_p1, addr_p2, port_p2);
  kcpuv_init_send(sess_p2, addr_p1, port_p1);

  // mux
  kcpuv_mux mux_p1;
  kcpuv_mux mux_p2;

  kcpuv_mux_init(&mux_p1, sess_p1);
  kcpuv_mux_init(&mux_p2, sess_p2);

  kcpuv_mux_conn mux_p1_conn_p1;
  kcpuv_mux_conn mux_p1_conn_p2;
  kcpuv_mux_conn_init(&mux_p1, &mux_p1_conn_p1);
  kcpuv_mux_conn_init(&mux_p1, &mux_p1_conn_p2);

  int content_len = 4096;
  char *content = new char[content_len];
  memset(content, 65, content_len);

  kcpuv_mux_send(&mux_p1_conn_p1, content, content_len, KCPUV_MUX_CMD_PUSH);
  kcpuv_mux_send(&mux_p1_conn_p2, content, content_len, KCPUV_MUX_CMD_PUSH);
  kcpuv_mux_conn_listen(&mux_p1_conn_p1, on_data_return);

  kcpuv_mux_bind_connection(&mux_p2, on_p2_conn);

  // loop
  kcpuv_start_loop(kcpuv__update_kcp_sess);

  delete[] content;
  delete[] addr_p1;
  delete[] addr_p2;
  kcpuv_free(sess_p1);
  kcpuv_free(sess_p2);
  kcpuv_destruct();
}

} // namespace kcpuv_test