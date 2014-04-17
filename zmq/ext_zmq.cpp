#include <sys/types.h>
#include <ext/hash_map>

#include "hphp/runtime/base/base-includes.h"
#include "hphp/runtime/ext/extension.h"
#include "hphp/runtime/base/complex-types.h"

#include "zmq.hpp"

namespace HPHP {

class ZmqContextResource : public SweepableResourceData {
public:
    DECLARE_RESOURCE_ALLOCATION(ZmqContextResource)
    CLASSNAME_IS("zmq_context")
    virtual const String& o_getClassNameHook() const { return classnameof(); }

    explicit ZmqContextResource(int io_threads) {
        ctx = new zmq::context_t(io_threads);
    }
    virtual ~ZmqContextResource() { 
        close(); 
        delete ctx;
    }
    void close() {
        ctx->close();
    }
    zmq::context_t* getContext() { return ctx; }

private:
    zmq::context_t *ctx;
};

void ZmqContextResource::sweep() {
    close();
}

class ZmqSocketResource : public SweepableResourceData {
public:
    DECLARE_RESOURCE_ALLOCATION(ZmqSocketResource)
    CLASSNAME_IS("zmq_socket")
    virtual const String& o_getClassNameHook() const { return classnameof(); }

    explicit ZmqSocketResource(zmq::context_t* ctx, int type){
        sock = new zmq::socket_t(*ctx, type);
    }
    virtual ~ZmqSocketResource() { 
        close(); 
        delete sock;
    }
    void close() {
        sock->close();
    }
    zmq::socket_t* getSocket() { return sock; }

private:
    zmq::socket_t* sock;
};

void ZmqSocketResource::sweep() {
    close();
}

class PollItem {
public:
    explicit PollItem(zmq::socket_t* s, String i, int t){
        sock = s;
        id = i;
        type = t;
    }

    ~PollItem(){
        sock = nullptr;
    }

    int getType(){
        return type;
    }
    String getId(){
        return id;
    }
    zmq::socket_t* getSock(){
        return sock;
    }
private:
    zmq::socket_t* sock;
    String id;
    int type;
};

class ZmqPollResource : public SweepableResourceData {
public:
    DECLARE_RESOURCE_ALLOCATION(ZmqPollResource)
    CLASSNAME_IS("zmq_poll")
    virtual const String& o_getClassNameHook() const { return classnameof(); }
    void addPollItem(zmq::socket_t* sock, const String& id, int type){
        PollItem *item = new PollItem(sock, id, type);
        items.push_back(item);
    }

    std::vector<PollItem*> getPollItems(){
        return items;
    }

    void removePollItem(String id){
        std::vector<PollItem*>::iterator it;
        for(it = items.begin(); it != items.end(); ){
            if((*it)->getId().same(id)){
                delete *it;
                it = items.erase(it);
                break;
            }
        }
    }

    void clear(){
        std::vector<PollItem*>::iterator it;
        for(it = items.begin(); it != items.end(); ){
            delete *it;
            it = items.erase(it);
        }
    }

private:
    std::vector<PollItem*> items;
};

void ZmqPollResource::sweep() {
    clear();
}

Variant php_zmq_context_create(int64_t io_threads)
{
    try{
        return NEWOBJ(ZmqContextResource)(io_threads);
    }catch(std::exception& e){
        return false;
    }
}

int64_t php_zmq_context_get_opt(const Resource& context, int64_t option)
{
    auto ctx = context.getTyped<ZmqContextResource>()->getContext();
    return ctx->get_opt(option);
}

int64_t php_zmq_context_set_opt(const Resource& context, int64_t option, int64_t value)
{ 
   try{
        auto ctx = context.getTyped<ZmqContextResource>()->getContext();
       return ctx->set_opt(option, value);
   }catch(std::exception& e){
       return -1;
   }
}

int64_t php_zmq_socket_connect(const Resource& socket, const String& dsn)
{
   try{
        auto sock = socket.getTyped<ZmqSocketResource>()->getSocket();
        sock->connect(dsn.c_str());
        return 0;
   }catch(std::exception& e){
       return -1;
   }
}

Variant php_zmq_socket_create(const Resource& context, int64_t type)
{
    auto ctx = context.getTyped<ZmqContextResource>()->getContext();
    try{
        return NEWOBJ(ZmqSocketResource)(ctx, type);
    }catch(std::exception& e){
       return false;
    }
}

int64_t php_zmq_socket_disconnect(const Resource& socket, const String& dsn)
{
   try{
        auto sock = socket.getTyped<ZmqSocketResource>()->getSocket();
       sock->disconnect(dsn.c_str());
       return 0;
   }catch(std::exception& e){
       return -1;
   }
}

int64_t php_zmq_socket_bind(const Resource& socket, const String& dsn)
{
   try{
    auto sock = socket.getTyped<ZmqSocketResource>()->getSocket();
       sock->bind(dsn.c_str());
       return 0;
   }catch(std::exception& e){
       return -1;
   }
}

int64_t php_zmq_socket_unbind(const Resource& socket, const String& dsn)
{
   try{
    auto sock = socket.getTyped<ZmqSocketResource>()->getSocket();
       sock->unbind(dsn.c_str());
       return 0;
   }catch(std::exception& e){
       return -1;
   }
}

int64_t php_zmq_socket_send(const Resource& socket, const String& message, int64_t flags)
{
   try{
        zmq::message_t msg(message.length());
        memcpy(msg.data(), message.c_str(), message.length());
        auto sock = socket.getTyped<ZmqSocketResource>()->getSocket();
        bool rc = sock->send(msg, flags);
        return rc ? 0 : -1;
   }catch(std::exception& e){
       return -1;
   }
}

int64_t php_zmq_socket_recv(const Resource& socket, VRefParam message, int64_t flags)
{
   try{
        zmq::message_t msg;
        auto sock = socket.getTyped<ZmqSocketResource>()->getSocket();
        bool rc = sock->recv(&msg, flags);
        int size = msg.size();
        char m[size + 1];
        memcpy(m, msg.data(), size);
        m[size] = 0;
        message = String(m, size + 1, CopyString);
        return rc ? 0 : -1;
   }catch(std::exception& e){
       return -1;
   }
}

Variant php_zmq_poll_create()
{
    return NEWOBJ(ZmqPollResource);
}

int64_t php_zmq_poll_add(const Resource& poll, const Resource& socket, const String& id, int64_t type)
{
    auto pollRes = poll.getTyped<ZmqPollResource>();
    auto sock = socket.getTyped<ZmqSocketResource>()->getSocket();
    pollRes->addPollItem(sock, id, type);
    return 0;
}

int64_t php_zmq_poll_poll(const Resource& poll, int64_t timeout, VRefParam readSocketsId, VRefParam writeSocketsId, VRefParam errorSocketsId)
{
    Array r_arr;
   Array w_arr;
   Array e_arr;

  if (readSocketsId.isReferenced()) {
    r_arr = Array::Create();
  }

  if(writeSocketsId.isReferenced()){
    w_arr = Array::Create();
  }

  if(errorSocketsId.isReferenced()){
    e_arr = Array::Create();
  }

  SCOPE_EXIT {
   if (readSocketsId.isReferenced()) readSocketsId = r_arr; 
   if (writeSocketsId.isReferenced()) writeSocketsId = w_arr; 
   if (errorSocketsId.isReferenced()) errorSocketsId = e_arr; 
};

    auto pollRes = poll.getTyped<ZmqPollResource>();
    std::vector<PollItem*> items = pollRes->getPollItems();
    int size = items.size();
   

    zmq_pollitem_t *items_t = (zmq_pollitem_t*) malloc(sizeof(struct zmq_pollitem_t) * size);

   int i;
   for(i = 0; i < items.size(); i++){
       memset(&items_t[i], 0, sizeof(struct zmq_pollitem_t));
       items_t[i].socket = *items[i]->getSock();
       items_t[i].events = items[i]->getType();
   }

   try{
       int rc = zmq::poll(items_t, items.size(), timeout);
       if (rc > 0) {
           for (i = 0; i < items.size(); i++) {
               if (items_t[i].revents & ZMQ_POLLIN) {
                   r_arr.append(items[i]->getId());
               }

               if (items_t[i].revents & ZMQ_POLLOUT) {
                   w_arr.append(items[i]->getId());
               }

               if (items_t[i].revents & ZMQ_POLLERR) {
                   e_arr.append(items[i]->getId());
               }
           }
       }
       free(items_t);

       return rc;
   }catch(std::exception& e){
    printf("%s\n", e.what());
       return -1;
   }
}

int64_t php_zmq_poll_remove(const Resource& poll, const String& id)
{
    auto pollRes = poll.getTyped<ZmqPollResource>();
    pollRes->removePollItem(id);
    return 0;
}

int64_t php_zmq_poll_clear(const Resource& poll)
{
    auto pollRes = poll.getTyped<ZmqPollResource>();
    pollRes->clear();
    return 0;
}

int64_t php_zmq_socket_set_opt(const Resource& socket, int64_t key, const Variant& value)
{
    try{
        auto sock = socket.getTyped<ZmqSocketResource>()->getSocket();

        switch(key){
            case ZMQ_SNDHWM:
            {
                int v = value.toInt32();
                sock->setsockopt(ZMQ_SNDHWM, &v, sizeof(int));
                }
            break;
            case ZMQ_RCVHWM:
            {
                int v = value.toInt32();
                sock->setsockopt(ZMQ_RCVHWM, &v, sizeof(int));
                }
            break;
            case ZMQ_AFFINITY:
            {
                uint64_t v = value.toInt64();
                sock->setsockopt(ZMQ_AFFINITY, &v, sizeof(uint64_t));
                }
            break;
            case ZMQ_SUBSCRIBE:
            {
                String v = value.toString();
                sock->setsockopt(ZMQ_SUBSCRIBE, v.c_str(), v.length());
                }
            break;
            case ZMQ_UNSUBSCRIBE:
            {
                String v = value.toString();
                sock->setsockopt(ZMQ_UNSUBSCRIBE, v.c_str(), v.length());
                }
            break;
            case ZMQ_IDENTITY:
            {
                String v = value.toString();
                sock->setsockopt(ZMQ_IDENTITY, v.c_str(), v.length());
                }
            break;
            case ZMQ_RATE:
            {
                int v = value.toInt32();
                sock->setsockopt(ZMQ_RATE, &v, sizeof(int));
                }
            break;
            case ZMQ_RECOVERY_IVL:
            {
                int v = value.toInt32();
                sock->setsockopt(ZMQ_RECOVERY_IVL, &v, sizeof(int));
                }
            break;
            case ZMQ_SNDBUF:
            {
                int v = value.toInt32();
                sock->setsockopt(ZMQ_SNDBUF, &v, sizeof(int));
                }
            break;
            case ZMQ_RCVBUF:
            {
                int v = value.toInt32();
                sock->setsockopt(ZMQ_RCVBUF, &v, sizeof(int));
                }
            break;
            case ZMQ_LINGER:
            {
                int v = value.toInt32();
                sock->setsockopt(ZMQ_LINGER, &v, sizeof(int));
                }
            break;
            case ZMQ_RECONNECT_IVL:
            {
                int v = value.toInt32();
                sock->setsockopt(ZMQ_RECONNECT_IVL, &v, sizeof(int));
                }
            break;
            case ZMQ_RECONNECT_IVL_MAX:
            {
                int v = value.toInt32();
                sock->setsockopt(ZMQ_RECONNECT_IVL_MAX, &v, sizeof(int));
                }
            break;
            case ZMQ_BACKLOG:
            {
                int v = value.toInt32();
                sock->setsockopt(ZMQ_BACKLOG, &v, sizeof(int));
                }
            break;
            case ZMQ_MAXMSGSIZE:
            {
                int64_t v = value.toInt64();
                sock->setsockopt(ZMQ_MAXMSGSIZE, &v, sizeof(int64_t));
                }
            break;
            case ZMQ_MULTICAST_HOPS:
            {
                int v = value.toInt32();
                sock->setsockopt(ZMQ_MULTICAST_HOPS, &v, sizeof(int));
                }
            break;
            case ZMQ_RCVTIMEO:
            {
                int v = value.toInt32();
                sock->setsockopt(ZMQ_RCVTIMEO, &v, sizeof(int));
                }
            break;
            case ZMQ_SNDTIMEO:
            {
                int v = value.toInt32();
                sock->setsockopt(ZMQ_SNDTIMEO, &v, sizeof(int));
                }
            break;
            case ZMQ_IPV4ONLY:
            {
                int v = value.toInt32();
                sock->setsockopt(ZMQ_IPV4ONLY, &v, sizeof(int));
                }
            break;
            case ZMQ_DELAY_ATTACH_ON_CONNECT:
            {
                int v = value.toInt32();
                sock->setsockopt(ZMQ_DELAY_ATTACH_ON_CONNECT, &v, sizeof(int));
                }
            break;
            case ZMQ_ROUTER_MANDATORY:
            {
                int v = value.toInt32();
                sock->setsockopt(ZMQ_ROUTER_MANDATORY, &v, sizeof(int));
                }
            break;
            case ZMQ_XPUB_VERBOSE:
            {
                int v = value.toInt32();
                sock->setsockopt(ZMQ_XPUB_VERBOSE, &v, sizeof(int));
                }
            break;
            case ZMQ_TCP_KEEPALIVE:
            {
                int v = value.toInt32();
                sock->setsockopt(ZMQ_TCP_KEEPALIVE, &v, sizeof(int));
                }
            break;
            case ZMQ_TCP_KEEPALIVE_IDLE:
            {
                int v = value.toInt32();
                sock->setsockopt(ZMQ_TCP_KEEPALIVE_IDLE, &v, sizeof(int));
                }
            break;
            case ZMQ_TCP_KEEPALIVE_CNT:
            {
                int v = value.toInt32();
                sock->setsockopt(ZMQ_TCP_KEEPALIVE_CNT, &v, sizeof(int));
                }
            break;
            case ZMQ_TCP_KEEPALIVE_INTVL:
            {
                int v = value.toInt32();
                sock->setsockopt(ZMQ_TCP_KEEPALIVE_INTVL, &v, sizeof(int));
                }
            break;
            case ZMQ_TCP_ACCEPT_FILTER:
            {
                String v = value.toString();
                sock->setsockopt(ZMQ_TCP_ACCEPT_FILTER, v.c_str(), v.length());
                }
            break;
            case ZMQ_RCVMORE:
            {
                int64_t v = value.toInt64();
                sock->setsockopt(ZMQ_RCVMORE, &v, sizeof(int64_t));
            }
            break;
            default:
                break;
        }
           
           return 0;
       }catch(std::exception& e){
           return -1;
       }
}

Variant php_zmq_socket_get_opt(const Resource& socket, int64_t key)
{
    try{
        auto sock = socket.getTyped<ZmqSocketResource>()->getSocket();

        switch(key){
            case ZMQ_SNDHWM:
            {
                int v;
                size_t size = sizeof(int);
                sock->getsockopt(ZMQ_SNDHWM, &v, &size);
                return v;
                }
            break;
            case ZMQ_RCVHWM:
            {
                int v;
                size_t size = sizeof(int);
                sock->getsockopt(ZMQ_RCVHWM, &v, &size);
                return v;
                }
            break;
            case ZMQ_AFFINITY:
            {
                uint64_t v;
                size_t size = sizeof(uint64_t);
                sock->getsockopt(ZMQ_AFFINITY, &v, &size);
                return v;
                }
            break;
            case ZMQ_SUBSCRIBE:
            {
                char v[255];
                size_t size = 255;
                sock->getsockopt(ZMQ_SUBSCRIBE, v, &size);
                return String(v, size, CopyString);
                }
            break;
            case ZMQ_UNSUBSCRIBE:
            {
                char v[255];
                size_t size = 255;
                sock->getsockopt(ZMQ_UNSUBSCRIBE, v, &size);
                return String(v, size, CopyString);
                }
            break;
            case ZMQ_IDENTITY:
            {
                char v[255];
                size_t size = 255;
                sock->getsockopt(ZMQ_IDENTITY, v, &size);
                return String(v, size, CopyString);
                }
            break;
            case ZMQ_RATE:
            {
                int v;
                size_t size = sizeof(int);
                sock->getsockopt(ZMQ_RATE, &v, &size);
                return v;
                }
            break;
            case ZMQ_RECOVERY_IVL:
            {
                int v;
                size_t size = sizeof(int);
                sock->getsockopt(ZMQ_RECOVERY_IVL, &v, &size);
                return v;
                }
            break;
            case ZMQ_SNDBUF:
            {
                int v;
                size_t size = sizeof(int);
                sock->getsockopt(ZMQ_SNDBUF, &v, &size);
                return v;
                }
            break;
            case ZMQ_RCVBUF:
            {
                int v;
                size_t size = sizeof(int);
                sock->getsockopt(ZMQ_RCVBUF, &v, &size);
                return v;
                }
            break;
            case ZMQ_LINGER:
            {
                int v;
                size_t size = sizeof(int);
                sock->getsockopt(ZMQ_LINGER, &v, &size);
                return v;
                }
            break;
            case ZMQ_RECONNECT_IVL:
            {
                int v;
                size_t size = sizeof(int);
                sock->getsockopt(ZMQ_RECONNECT_IVL, &v, &size);
                return v;
                }
            break;
            case ZMQ_RECONNECT_IVL_MAX:
            {
                int v;
                size_t size = sizeof(int);
                sock->getsockopt(ZMQ_RECONNECT_IVL_MAX, &v, &size);
                return v;
                }
            break;
            case ZMQ_BACKLOG:
            {
                int v;
                size_t size = sizeof(int);
                sock->getsockopt(ZMQ_BACKLOG, &v, &size);
                return v;
                }
            break;
            case ZMQ_MAXMSGSIZE:
            {
                int64_t v;
                size_t size = sizeof(int64_t);
                sock->getsockopt(ZMQ_MAXMSGSIZE, &v, &size);
                return v;
                }
            break;
            case ZMQ_MULTICAST_HOPS:
            {
                int v;
                size_t size = sizeof(int);
                sock->getsockopt(ZMQ_MULTICAST_HOPS, &v, &size);
                return v;
                }
            break;
            case ZMQ_RCVTIMEO:
            {
                int v;
                size_t size = sizeof(int);
                sock->getsockopt(ZMQ_RCVTIMEO, &v, &size);
                return v;
                }
            break;
            case ZMQ_SNDTIMEO:
            {
                int v;
                size_t size = sizeof(int);
                sock->getsockopt(ZMQ_SNDTIMEO, &v, &size);
                return v;
                }
            break;
            case ZMQ_IPV4ONLY:
            {
                int v;
                size_t size = sizeof(int);
                sock->getsockopt(ZMQ_IPV4ONLY, &v, &size);
                return v;
                }
            break;
            case ZMQ_DELAY_ATTACH_ON_CONNECT:
            {
                int v;
                size_t size = sizeof(int);
                sock->getsockopt(ZMQ_DELAY_ATTACH_ON_CONNECT, &v, &size);
                return v;
                }
            break;
            case ZMQ_ROUTER_MANDATORY:
            {
                int v;
                size_t size = sizeof(int);
                sock->getsockopt(ZMQ_ROUTER_MANDATORY, &v, &size);
                return v;
                }
            break;
            case ZMQ_XPUB_VERBOSE:
            {
                int v;
                size_t size = sizeof(int);
                sock->getsockopt(ZMQ_XPUB_VERBOSE, &v, &size);
                return v;
                }
            break;
            case ZMQ_TCP_KEEPALIVE:
            {
                int v;
                size_t size = sizeof(int);
                sock->getsockopt(ZMQ_TCP_KEEPALIVE, &v, &size);
                return v;
                }
            break;
            case ZMQ_TCP_KEEPALIVE_IDLE:
            {
                int v;
                size_t size = sizeof(int);
                sock->getsockopt(ZMQ_TCP_KEEPALIVE_IDLE, &v, &size);
                return v;
                }
            break;
            case ZMQ_TCP_KEEPALIVE_CNT:
            {
                int v;
                size_t size = sizeof(int);
                sock->getsockopt(ZMQ_TCP_KEEPALIVE_CNT, &v, &size);
                return v;
                }
            break;
            case ZMQ_TCP_KEEPALIVE_INTVL:
            {
                int v;
                size_t size = sizeof(int);
                sock->getsockopt(ZMQ_TCP_KEEPALIVE_INTVL, &v, &size);
                return v;
                }
            break;
            case ZMQ_TCP_ACCEPT_FILTER:
            {
                char v[255];
                size_t size = 255;
                sock->getsockopt(ZMQ_TCP_ACCEPT_FILTER, v, &size);
                return String(v, size, CopyString);
                }
            break;
            case ZMQ_RCVMORE:
            {
                int64_t v;
                size_t size = sizeof(int64_t);
                sock->getsockopt(ZMQ_RCVMORE, &v, &size);
                return v;
            }
            break;
            default:
                return 0;
                break;
        }
       }catch(std::exception& e){
           return -1;
       }
       return 0;
}

static Variant HHVM_FUNCTION(zmq_context_create, int64_t io_threads)
{
    return php_zmq_context_create(io_threads);
}

static int64_t HHVM_FUNCTION(zmq_context_get_opt, const Resource& context, int64_t option)
{
   return php_zmq_context_get_opt(context, option);
}

static int64_t HHVM_FUNCTION(zmq_context_set_opt, const Resource& context, int64_t option, int64_t value)
{
   return php_zmq_context_set_opt(context, option, value);
}

static int64_t HHVM_FUNCTION(zmq_socket_connect, const Resource& socket, const String& dsn)
{
   return php_zmq_socket_connect(socket, dsn);
}

static Variant HHVM_FUNCTION(zmq_socket_create, const Resource& context, int64_t type)
{
   return php_zmq_socket_create(context, type);
}

static int64_t HHVM_FUNCTION(zmq_socket_disconnect, const Resource& socket, const String& dsn)
{
   return php_zmq_socket_disconnect(socket, dsn);
}

static int64_t HHVM_FUNCTION(zmq_socket_bind, const Resource& socket, const String& dsn)
{
   return php_zmq_socket_bind(socket, dsn);
}

static int64_t HHVM_FUNCTION(zmq_socket_unbind, const Resource& socket, const String& dsn)
{
   return php_zmq_socket_unbind(socket, dsn);
}

static int64_t HHVM_FUNCTION(zmq_socket_send, const Resource& socket, const String& message, int64_t flags)
{
   return php_zmq_socket_send(socket, message, flags);
}

static int64_t HHVM_FUNCTION(zmq_socket_recv, const Resource& socket, VRefParam message, int64_t flags)
{
   return php_zmq_socket_recv(socket, message, flags);
}

static Variant HHVM_FUNCTION(zmq_poll_create)
{
    return php_zmq_poll_create();
}

static int64_t HHVM_FUNCTION(zmq_poll_add, const Resource& poll, const Resource& socket, const String& id, int64_t type)
{
    return php_zmq_poll_add(poll, socket, id, type);
}

static int64_t HHVM_FUNCTION(zmq_poll_poll, const Resource& poll, int64_t timeout, VRefParam readSocketsId, VRefParam writeSocketsId, VRefParam errorSocketsId)
{
   return php_zmq_poll_poll(poll, timeout, readSocketsId, writeSocketsId, errorSocketsId);
}

static int64_t HHVM_FUNCTION(zmq_poll_remove, const Resource& poll, const String& id)
{
    return php_zmq_poll_remove(poll, id);
}

static int64_t HHVM_FUNCTION(zmq_poll_clear, const Resource& poll)
{
    return php_zmq_poll_clear(poll);
}

static int64_t HHVM_FUNCTION(zmq_socket_set_opt, const Resource& socket, int64_t key, const Variant& value)
{
    return php_zmq_socket_set_opt(socket, key, value);
}

static Variant HHVM_FUNCTION(zmq_socket_get_opt, const Resource& socket, int64_t key)
{
    return php_zmq_socket_get_opt(socket, key);
}

class zmqExtension : public Extension {
public:
    zmqExtension() : Extension("zmq") {}

    virtual void moduleInit() {
        HHVM_FE(zmq_context_create);
        HHVM_FE(zmq_context_get_opt);
        HHVM_FE(zmq_context_set_opt);
        HHVM_FE(zmq_socket_connect);
        HHVM_FE(zmq_socket_create);
        HHVM_FE(zmq_socket_disconnect);
        HHVM_FE(zmq_socket_bind);
        HHVM_FE(zmq_socket_unbind);
        HHVM_FE(zmq_socket_send);
        HHVM_FE(zmq_socket_recv);
        HHVM_FE(zmq_socket_set_opt);
        HHVM_FE(zmq_socket_get_opt);
        HHVM_FE(zmq_poll_poll);
        HHVM_FE(zmq_poll_create);
        HHVM_FE(zmq_poll_add);
        HHVM_FE(zmq_poll_remove);
        HHVM_FE(zmq_poll_clear);

        loadSystemlib();
    }
} s_zmq_extension;

// Uncomment for non-bundled module
HHVM_GET_MODULE(zmq);

//////////////////////////////////////////////////////////////////////////////
} // namespace HPHP
