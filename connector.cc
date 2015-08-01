#include <node.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>

#include <linux/connector.h>
#include <linux/netlink.h>
#include <linux/cn_proc.h>

#include "connector.h"

using namespace v8;

static void IoEvent (uv_poll_t* watcher, int status, int revents);

Persistent<Function> Connector::constructor;

void Connector::Init(Handle<Object> exports) {
    Isolate* isolate = Isolate::GetCurrent();

    Local<FunctionTemplate> tpl = FunctionTemplate::New(isolate, New);
    tpl->SetClassName(String::NewFromUtf8(isolate, "ConnectorWrap"));
    tpl->InstanceTemplate()->SetInternalFieldCount(1);

    NODE_SET_PROTOTYPE_METHOD(tpl, "connect", Connect);
    NODE_SET_PROTOTYPE_METHOD(tpl, "close", Close);

    constructor.Reset(isolate, tpl->GetFunction());
    exports->Set(String::NewFromUtf8(isolate, "ConnectorWrap"),
            tpl->GetFunction());
}

void Connector::New(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    Connector *obj = new Connector();

    obj->sockfd = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_CONNECTOR);
    obj->Wrap(args.This());
    args.GetReturnValue().Set(args.This());
}

void initAll(Handle<Object> exports) {
    Connector::Init(exports);
}

Connector::Connector() {
}

void Connector::Close(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    Connector *that = ObjectWrap::Unwrap<Connector>(args.Holder());

    struct nlmsghdr *nl_hdr;
    struct cn_msg *cn_hdr;
    char buff[BUFF_SIZE];

    enum proc_cn_mcast_op *mcop_msg;

    nl_hdr = (struct nlmsghdr *)buff;
    cn_hdr = (struct cn_msg *)NLMSG_DATA(nl_hdr);
    mcop_msg = (enum proc_cn_mcast_op*)&cn_hdr->data[0];

    memset(buff, 0, sizeof(buff));
    *mcop_msg = PROC_CN_MCAST_IGNORE;

    /* fill the netlink header */
    nl_hdr->nlmsg_len = SEND_MESSAGE_LEN;
    nl_hdr->nlmsg_type = NLMSG_DONE;
    nl_hdr->nlmsg_flags = 0;
    nl_hdr->nlmsg_seq = 0;
    nl_hdr->nlmsg_pid = getpid();
    /* fill the connector header */
    cn_hdr->id.idx = CN_IDX_PROC;
    cn_hdr->id.val = CN_VAL_PROC;
    cn_hdr->seq = 0;
    cn_hdr->ack = 0;
    cn_hdr->len = sizeof(enum proc_cn_mcast_op);
    send(that->sockfd, nl_hdr, nl_hdr->nlmsg_len, 0);
    uv_poll_stop(that->poll_watcher_);
    close(that->sockfd);
}

void Connector::Connect(const FunctionCallbackInfo<Value>& args) {
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);

    Connector *that = ObjectWrap::Unwrap<Connector>(args.Holder());

    struct sockaddr_nl my_nla;
    struct nlmsghdr *nl_hdr;
    struct cn_msg *cn_hdr;
    char buff[BUFF_SIZE];
    int err;

    enum proc_cn_mcast_op *mcop_msg;

    my_nla.nl_family = AF_NETLINK;
    my_nla.nl_groups = CN_IDX_PROC;
    my_nla.nl_pid = getpid();

    err = bind(that->sockfd, (struct sockaddr *)&my_nla, sizeof(my_nla));
    if (err != 0) {
        perror("bind failed");
    }

    nl_hdr = (struct nlmsghdr *)buff;
    cn_hdr = (struct cn_msg *)NLMSG_DATA(nl_hdr);
    mcop_msg = (enum proc_cn_mcast_op*)&cn_hdr->data[0];

    memset(buff, 0, sizeof(buff));
    *mcop_msg = PROC_CN_MCAST_LISTEN;

    /* fill the netlink header */
    nl_hdr->nlmsg_len = SEND_MESSAGE_LEN;
    nl_hdr->nlmsg_type = NLMSG_DONE;
    nl_hdr->nlmsg_flags = 0;
    nl_hdr->nlmsg_seq = 0;
    nl_hdr->nlmsg_pid = getpid();
    /* fill the connector header */
    cn_hdr->id.idx = CN_IDX_PROC;
    cn_hdr->id.val = CN_VAL_PROC;
    cn_hdr->seq = 0;
    cn_hdr->ack = 0;
    cn_hdr->len = sizeof(enum proc_cn_mcast_op);
    if (send(that->sockfd, nl_hdr, nl_hdr->nlmsg_len, 0) != nl_hdr->nlmsg_len) {
        perror("send failed");	
        printf("failed to send proc connector mcast ctl op!\n");
    }

    that->poll_watcher_ = new uv_poll_t;
    uv_poll_init_socket (uv_default_loop (), that->poll_watcher_,
            that->sockfd);
    that->poll_watcher_->data = that;
    uv_poll_start (that->poll_watcher_, UV_READABLE, IoEvent);
}

void Connector::handle_msg (struct cn_msg *cn_hdr)
{
    Isolate* isolate = Isolate::GetCurrent();
    HandleScope scope(isolate);
    struct proc_event *ev = (struct proc_event *)cn_hdr->data;

    switch(ev->what){
        case proc_event::PROC_EVENT_FORK:
            {
                const unsigned argc = 5;
                Local<Value> argv[argc] = {
                    String::NewFromUtf8(isolate, "fork"),
                    Number::New(isolate, ev->event_data.fork.parent_pid),
                    Number::New(isolate, ev->event_data.fork.parent_tgid),
                    Number::New(isolate, ev->event_data.fork.child_pid),
                    Number::New(isolate, ev->event_data.fork.child_tgid)
                };
                node::MakeCallback(isolate, this->handle(), "emit", argc, argv);
            }
            break;
        case proc_event::PROC_EVENT_EXEC:
            {
                const unsigned argc = 3;
                Local<Value> argv[argc] = {
                    String::NewFromUtf8(isolate, "exec"),
                    Number::New(isolate, ev->event_data.exec.process_pid),
                    Number::New(isolate, ev->event_data.exec.process_tgid)
                };
                node::MakeCallback(isolate, this->handle(), "emit", argc, argv);
            }
            break;
        case proc_event::PROC_EVENT_EXIT:
            {
                const unsigned argc = 4;
                Local<Value> argv[argc] = {
                    String::NewFromUtf8(isolate, "exit"),
                    Number::New(isolate, ev->event_data.exit.process_pid),
                    Number::New(isolate, ev->event_data.exit.process_tgid),
                    Number::New(isolate, ev->event_data.exit.exit_code)
                };
                node::MakeCallback(isolate, this->handle(), "emit", argc, argv);
            }
            break;
        case proc_event::PROC_EVENT_UID:
            {
                const unsigned argc = 5;
                Local<Value> argv[argc] = {
                    String::NewFromUtf8(isolate, "uid"),
                    Number::New(isolate, ev->event_data.id.process_pid),
                    Number::New(isolate, ev->event_data.id.process_tgid),
                    Number::New(isolate, ev->event_data.id.r.ruid),
                    Number::New(isolate, ev->event_data.id.e.euid)
                };
                node::MakeCallback(isolate, this->handle(), "emit", argc, argv);
            }
            break;
        default:
            break;
    }
}


void Connector::HandleIOEvent (int status, int revents) {
    char buff[BUFF_SIZE];

    struct sockaddr_nl kern_nla, from_nla;
    size_t recv_len;
    socklen_t from_nla_len;
    struct cn_msg *cn_hdr;

    kern_nla.nl_family = AF_NETLINK;
    kern_nla.nl_groups = CN_IDX_PROC;
    kern_nla.nl_pid = 1;

    memset(buff, 0, sizeof(buff));
    do {
        struct nlmsghdr *nlh = (struct nlmsghdr*)buff;
        memcpy(&from_nla, &kern_nla, sizeof(from_nla));
        recv_len = recvfrom(this->sockfd, buff, BUFF_SIZE, 0,
                (struct sockaddr*)&from_nla, &from_nla_len);
        if (from_nla.nl_pid != 0)
            continue;
        if (recv_len < 1)
            continue;
        while (NLMSG_OK(nlh, recv_len)) {
            cn_hdr = (cn_msg*)NLMSG_DATA(nlh);
            if (nlh->nlmsg_type == NLMSG_NOOP)
                continue;
            if ((nlh->nlmsg_type == NLMSG_ERROR) ||
                    (nlh->nlmsg_type == NLMSG_OVERRUN))
                break;
            handle_msg(cn_hdr);
            if (nlh->nlmsg_type == NLMSG_DONE)
                break;
            nlh = NLMSG_NEXT(nlh, recv_len);
        }
    } while(0);
}

Connector::~Connector() {
    close(sockfd);
}

static void IoEvent (uv_poll_t* watcher, int status, int revents) {
    Connector *socket = static_cast<Connector*>(watcher->data);
    socket->HandleIOEvent (status, revents);
}

NODE_MODULE(connector, initAll)
