#include <node.h>
#include <node_object_wrap.h>
#include <uv.h>

#define SEND_MESSAGE_LEN (NLMSG_LENGTH(sizeof(struct cn_msg) + \
            sizeof(enum proc_cn_mcast_op)))
#define RECV_MESSAGE_LEN (NLMSG_LENGTH(sizeof(struct cn_msg) + \
            sizeof(struct proc_event)))

#define SEND_MESSAGE_SIZE    (NLMSG_SPACE(SEND_MESSAGE_LEN))
#define RECV_MESSAGE_SIZE    (NLMSG_SPACE(RECV_MESSAGE_LEN))

#define max(x,y) ((y)<(x)?(x):(y))
#define min(x,y) ((y)>(x)?(x):(y))

#define BUFF_SIZE (max(max(SEND_MESSAGE_SIZE, RECV_MESSAGE_SIZE), 1024))
#define MIN_RECV_SIZE (min(SEND_MESSAGE_SIZE, RECV_MESSAGE_SIZE))

using namespace v8;

class Connector : public node::ObjectWrap {
    public:
        static void Init(v8::Handle<v8::Object> exports);
        void HandleIOEvent (int status, int revents);
    private:
        Connector();
        ~Connector();

        void handle_msg (struct cn_msg *cn_hdr);
        static v8::Persistent<v8::Function> constructor;
        static void New(const FunctionCallbackInfo<Value>& args);
        static void Connect(const FunctionCallbackInfo<Value>& args);
        static void Close(const FunctionCallbackInfo<Value>& args);
        /* */
        int sockfd;
        uv_poll_t *poll_watcher_;
};
