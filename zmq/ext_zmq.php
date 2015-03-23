<?hh
/**
 *
 * ZMQ Constants
 */
class ZMQ {
  /**
   *
   * zmq socket type
   */
  const SOCKET_PAIR = 0;
  const SOCKET_PUB = 1;
  const SOCKET_SUB = 2;
  const SOCKET_REQ =3;
  const SOCKET_REP =4;
  const SOCKET_XREQ = 5;
  const SOCKET_XREP = 6;
  const SOCKET_PUSH = 8;
  const SOCKET_PULL =7;
  const SOCKET_ROUTER =5;
  const SOCKET_DEALER =6;
  const SOCKET_XPUB =9;
  const SOCKET_XSUB =10;

  /**
   *
   * zmq socket option
   */
  const SOCKOPT_SNDHWM = 23;
  const SOCKOPT_RCVHWM = 24;
  const SOCKOPT_AFFINITY = 4;
  const SOCKOPT_IDENTITY = 5;
  const SOCKOPT_SUBSCRIBE = 6;
  const SOCKOPT_UNSUBSCRIBE = 7;
  const SOCKOPT_RATE = 8;
  const SOCKOPT_RECOVERY_IVL = 9;
  const SOCKOPT_RECONNECT_IVL = 18;
  const SOCKOPT_RECONNECT_IVL_MAX = 21;
  const SOCKOPT_SNDBUF = 11;
  const SOCKOPT_RCVBUF = 12;
  const SOCKOPT_RCVMORE = 13;
  const SOCKOPT_LINGER = 17;
  const SOCKOPT_BACKLOG = 19;
  const SOCKOPT_MAXMSGSIZE = 22;
  const SOCKOPT_SNDTIMEO = 28;
  const SOCKOPT_RCVTIMEO = 27;
  const SOCKOPT_IPV4ONLY = 31;
  const SOCKOPT_TCP_KEEPALIVE = 34;
  const SOCKOPT_TCP_KEEPALIVE_IDLE = 36;
  const SOCKOPT_TCP_KEEPALIVE_CNT = 35;
  const SOCKOPT_TCP_KEEPALIVE_INTVL = 37;
  const SOCKOPT_TCP_ACCEPT_FILTER = 38;
  const SOCKOPT_DELAY_ATTACH_ON_CONNECT = 39;
  const SOCKOPT_XPUB_VERBOSE = 40;

  /**
   *
   * ZMQ Socket type
   */
  const POLL_IN = 1;
  const POLL_OUT = 2;

  const ZMQ_IO_THREADS = 1;
  const ZMQ_MAX_SOCKETS = 2;

  /*  message options  */ 
  const MODE_NOBLOCK = 1;
  const MODE_DONTWAIT = 1;
  const MODE_SNDMORE = 2;

    /* Methods */
  private function __construct(){}
}

class ZMQException extends Exception {}
class ZMQInvalidArgumentException extends InvalidArgumentException {}

class ZMQContext {

   private bool $is_persistent;
   private resource $context;
   private array $persistent_sockets = array();

   /**
    * Build a new ZMQContext. Persistent context is required for building
    * persistent sockets.
    *
    *
    * @param integer $io_threads     Number of io threads
    * @param boolean $is_persistent  Whether the context is persistent
    *
    * @return void
    */
   public function __construct(int $io_threads = 1, bool $is_persistent = true)
   {
       $this->is_persistent = $is_persistent;
       $ctx = zmq_context_create($io_threads);
       if(!$ctx){
           throw new Exception("create zmq context failed");
       }
       $this->context = $ctx;
   }

   /**
    * Construct a new ZMQ object. The extending class must call this method.
    * The type is one of the ZMQ::SOCKET_* constants.
    * Persistent id allows reusing the socket over multiple requests.
    * If persistent_id parameter is specific the type parameter is ignored and the
    * socket is of type that it was initially created with. Persistent context must
    * be enabled for persistent_id to work. Setting incorrect socket type might
    * cause failure later in connect/bind/setSockOpt.
    *
    * @param integer $type              The type of the socket
    * @param string  $persistent_id     The persistent id. Can be used to create
    *                                   persistent connections
    * @param function   $on_new_socket     Called when a new socket is created
    * @throws ZMQException
    * @return ZMQSocket
    */
   public function getSocket(int $type, string $persistent_id = null, mixed $on_new_socket = null): mixed
   {
       //if context not persistent
       if(!$this->is_persistent && !empty($persistent_id)){
           throw new ZMQInvalidArgumentException('context should be persistent');
       }

       if(!empty($persistent_id)){
           if(isset($this->persistent_sockets[$persistent_id])){
               return $this->persistent_sockets[$persistent_id];
           }

           $zmqSocket = new ZMQSocket($this, $type, $persistent_id, $on_new_socket);
           $this->persistent_sockets[$persistent_id] = $zmqSocket;
           return $zmqSocket;
       }

       $zmqSocket = new ZMQSocket($this, $type, null, $on_new_socket);
       return $zmqSocket;
   }

   /**
    * Whether the context is persistent
    *
    * @return boolean
    */
   public function isPersistent() : bool
   {
       return $this->is_persistent;
   }

   public function getOpt(int $key) : int
   {
       return zmq_context_get_opt($this->context, $key);
   }

   public function setOpt(int $key , int $value) : int
   {
       return zmq_context_set_opt($this->context, $key, $value);
   }

   public function getContext(): ?resource
   {
      return $this->context;
   }
}

class ZMQSocket {

   private resource $socket;
   private string $persistent_id;
   private array $conn_dsns = array();
   private array $bind_dsns = array();
   private int $type;
   /**
    * Construct a new ZMQ object. The extending class must call this method.
    * The type is one of the ZMQ::SOCKET_* constants.
    * Persistent id allows reusing the socket over multiple requests.
    * If persistent_id parameter is specific the type parameter is ignored and the
    * socket is of type that it was initially created with. Persistent context must
    * be enabled for persistent_id to work. Setting incorrect socket type might
    * cause failure later in connect/bind/setSockOpt.
    *
    * @param ZMQContext $context           ZMQContext to build this object
    * @param integer    $type              The type of the socket
    * @param string     $persistent_id     The persistent id. Can be used to create
    *                                      persistent connections
    * @param function   $on_new_socket     Called when a new socket is created
    * @throws ZMQException
    * @return void
    */
   public function __construct(ZMQContext $context, int $type, ?string $persistent_id = null, mixed $on_new_socket = null)
   {
       //if persistent id
       if(!empty($persistent_id)){
           $this->persistent_id = $persistent_id;
           
           $socket = zmq_socket_create($context->getContext(), $type);
           
           if($socket == -1){
               throw new ZMQException("create zmq socket failed");
           }

           $this->socket = $socket;
           if(!$this->setSockOpt(ZMQ::SOCKOPT_IDENTITY, $persistent_id)){
               throw new ZMQException("set zmq socket persistent id failed");
           }

           $this->persistent_id = $persistent_id;

       }else{
           $socket = zmq_socket_create($context->getContext(), $type);
           if($socket == -1){
               throw new ZMQException("create zmq socket failed");
           }
           $this->socket = $socket;
       }
       $this->type = $type;

       if($on_new_socket != null && is_callable($on_new_socket)){
          call_user_func($on_new_socket);
       }
   }

   /**
    * Sends a message to the queue.
    *
    * @param string  $message  The message to send
    * @param integer $flags    self::MODE_NOBLOCK or 0
    * @throws ZMQException if sending message fails
    *
    * @return ZMQ
    */
   public function send(string $message, int $flags = 0) : mixed
   {
       if(zmq_socket_send($this->socket, $message, $flags) != 0){
           throw new ZMQException("zmq socket send message " . $message . " failed");
       }

       return $this;
   }

   /**
    * Receives a message from the queue.
    *
    * @param integer $flags self::MODE_NOBLOCK or 0
    * @throws ZMQException if receiving fails.
    *
    * @return string
    */
   public function recv(int $flags = 0): ?string
   {
       if(zmq_socket_recv($this->socket, &$message, $flags) != 0){
           throw new ZMQException("zmq socket recv message failed");
       }
       return $message;
   }

   /**
    * Connect the socket to a remote endpoint. For more information about the dsn
    * see http://api.zeromq.org/zmq_connect.html. By default the method does not
    * try to connect if it has been already connected to the address specified by $dsn.
    *
    * @param string  $dsn   The connect dsn
    * @param boolean $force Tries to connect to end-point even if the object is already connected to the $dsn
    *
    * @throws ZMQException If connection fails
    * @return ZMQ
    */
   public function connect(string $dsn, bool $force = false) : mixed
   {
       if(!isset($this->conn_dsns[$dsn]) || $force){
           if(zmq_socket_connect($this->socket, $dsn) == 0){
               $this->conn_dsns[$dsn] = 1;
           }else{
               throw new ZMQException("zmq socket connect " . $dsn . "failed");
           }
       }
       return $this;
   }

   public function disconnect(string $dsn) : mixed
   {
       if(isset($this->conn_dsns[$dsn])){
           if(zmq_socket_disconnect($this->socket, $dsn) != 0){
               throw new ZMQException("zmq socket disconnect " . $dsn . " failed");
           }
       }
       return $this;
   }

   /**
    *
    * Binds the socket to an end-point. For more information about the dsn see
    * http://api.zeromq.org/zmq_connect.html. By default the method does not
    * try to bind if it has been already bound to the address specified by $dsn.
    *
    * @param string  $dsn   The bind dsn
    * @param boolean $force Tries to bind to end-point even if the object is already bound to the $dsn
    *
    * @throws ZMQException if binding fails
    * @return ZMQ
    */
   public function bind(string $dsn, bool $force = false) : mixed
   {
       if(!isset($this->bind_dsns[$dsn]) || $force){
           if(zmq_socket_bind($this->socket, $dsn) == 0){
               $this->bind_dsns[$dsn] = 1;
           }else{
               throw new ZMQException("zmq socket bind " . $dsn . "failed");
           }
       }
       return $this;
   }

   public function unbind(string $dsn) : mixed
   {
       if(isset($this->bind_dsns[$dsn])){
           if(zmq_socket_unbind($this->socket, $dsn) != 0){
               throw new ZMQException("zmq socket unbind " . $dsn . " failed");
           }
       }
       return $this;
   }


   /**
    * Sets a socket option. For more information about socket options see
    * http://api.zeromq.org/zmq_setsockopt.html
    *
    * @param integer $key   The option key
    * @param mixed   $value The option value
    *
    * @throws ZMQException
    * @return ZMQ
    */
    
   public function setSockOpt(int $key, mixed $value): void
   {
      if(zmq_socket_set_opt($this->socket, $key, $value) != 0){
          throw new ZMQException('zmq socket set option ' . $key . ' failed');
      }
      return $this;
   }

   /**
    * Gets a socket option. This method is available if ZMQ extension
    * has been compiled against ZMQ version 2.0.7 or higher
    *
    * @since 0MQ 2.0.7
    * @param integer $key The option key
    *
    * @throws ZMQException
    * @return mixed
    */
   public function getSockOpt(int $key): mixed
   {
      return zmq_socket_get_opt($this->socket, $key);
   }

   /**
    * Get endpoints where the socket is connected to. The return array
    * contains two sub-arrays: 'connect' and 'bind'
    *
    * @throws ZMQException
    * @return array
    */
   public function getEndpoints(): array
   {
       return array(
           'connect' => $this->conn_dsns,
           'bind' => $this->bind_dsns
       );
   }

   /**
    * Return the socket type. Returns one of ZMQ::SOCKET_* constants
    *
    * @throws ZMQException
    * @return integer
    */
   public function getSocketType(): int
   {
       return $this->type;
   }

   /**
    * Whether the socket is persistent
    *
    * @return boolean
    */
   public function isPersistent() : bool
   {
       return !empty($this->persistent_id);
   }

   public function getPersistentId() : ?string
   {
       return $this->persistent_id;
   }

   public function getSocket(): resource
   {
       return $this->socket;
   }
}

class ZMQPoll {

    private resource $poll; 
   private array $sockets;
   private array $errors;

   public function __construct()
   {
      $this->sockets = array();
      $this->errors = array();
      $poll = zmq_poll_create();
      if(!$poll){
        throw new ZMQException('create zmq poll failed');
      }
      $this->poll = $poll;
   }

   /**
    * Add a new object into the poll set. Returns the id for the object
    * in the pollset.
    *
    * @param ZMQ $object Object to add to set
    * @param integer $type Bit-mask of ZMQ::POLL_* constants
    *
    * @throws ZMQPollException if the object has not been initialized with polling
    * @return integer
    */
   public function add(ZMQSocket $object, int $type): ?string
   {
       if(!($object instanceof ZMQSocket)){
           throw new ZMQInvalidArgumentException("object should be instance of ZMQSocket");
       }

       $id = spl_object_hash($object);
       if(!isset($this->sockets[$id])){
            if(zmq_poll_add($this->poll, $object->getSocket(), $id, $type) == 0){
                $this->sockets[$id] = array(
                 'type' => $type,
                 'socket' => $object
              );
            }else{
              throw new ZMQException('zmq poll add socket failed');
            }
           
       }

       return $id;
   }

   /**
    * Execute the poll. Readable and writable sockets are returned
    * in the arrays passed by reference. If either of the given arrays
    * is null the events of that type are not returned. Returns an integer
    * indicated the amount of objects with events pending.
    *
    * @param array &$readable   array where to return the readable objects
    * @param array &$writable   array where to return the writable objects
    * @param integer $timeout   Timeout for poll in microseconds. -1 polls as long as one of the objects comes readable/writable
    *
    * @throws ZMQPollException if polling fails
    * @return integer
    */
   public function poll(array &$readable, array &$writable, int $timeout = -1): int
   {
       if(empty($this->sockets))
           return 0;

        $r_arr = array();
        $w_arr = array();
        
       $rc = zmq_poll_poll($this->poll, $timeout, &$r_arr, &$w_arr, $timeout);
       
       if($rc < 0){
           throw new ZMQException("zmq poll failed");
       }

       foreach ($r_arr as $id) {
         if(isset($this->sockets[$id])){
            $readable[] = $this->sockets[$id]['socket'];
         }
       }

       foreach ($w_arr as $id) {
         if(isset($this->sockets[$id])){
            $writable[] = $this->sockets[$id]['socket'];
         }
       }

       return $rc;
   }

   /**
    * Returns the ids of the objects that had ZMQ_POLLERR flag set on the last
    * poll call. This method does not clear the last errors and the errors are
    * cleared on next call to poll()
    *
    * @return array
    */
   public function getLastErrors(): array
   {
       return $this->errors;
   }

   /**
    * Removes an item from the poll object. The parameter can be ZMQ object,
    * resource or the string id returned by 'add' method. Returns true if the
    * item was removed and false if item had not been added to the poll object.
    *
    * @throws ZMQPollException if the poll object is empty
    * @throws ZMQPollException if $item parameter is object but not an instance of ZMQ
    *
    * @param mixed $item  The item to remove
    * @return boolean
    */
   public function remove(ZMQSocket $object): bool
   {
       $id = spl_object_hash($object);
       if(isset($this->sockets[$id])){
          if(zmq_poll_remove($this->poll, $id) == 0){
             unset($this->sockets[$id]);
            return true;
         }
       }
       return false;
   }

   /**
    * Counts the items in the poll object
    *
    * @return integer
    */
   public function count(): int
   {
       return count($this->sockets);
   }

   /**
    * Removes all items from the poll set
    *
    * @return ZMQPoll
    */
   public function clear(): ZMQPoll
   {
        if(zmq_poll_clear($this->poll) == 0){
          $this->sockets = array();
          $this->errors = array();
        }else{
          throw new ZMQException('zmq poll clear failed');
        }

       return $this;
   }

}

<<__Native>>
function zmq_context_create(int $io_threads) : mixed;

<<__Native>>
function zmq_socket_create(resource $context, int $type) : mixed;

<<__Native>>
function zmq_context_get_opt(resource $context, int $option): int;

<<__Native>>
function zmq_context_set_opt(resource $context, int $option, int $value): int;

<<__Native>>
function zmq_socket_connect(resource $socket, string $dsn): int;

<<__Native>>
function zmq_socket_disconnect(resource $socket, string $dsn): int;

<<__Native>>
function zmq_socket_bind(resource $socket, string $dsn): int;

<<__Native>>
function zmq_socket_unbind(resource $socket, string $dsn): int;

<<__Native>>
function zmq_socket_send(resource $socket, string $message, int $flags): int;

<<__Native>>
function zmq_socket_recv(resource $socket, mixed &$message, int $flags): int;

<<__Native>>
function zmq_socket_set_opt(resource $socket, int $key, mixed $value): int;

<<__Native>>
function zmq_socket_get_opt(resource $socket, int $key): mixed;

<<__Native>>
function zmq_poll_create(): mixed;

<<__Native>>
function zmq_poll_add(resource $poll, resource $socket, string $id, int $type): int;

<<__Native>>
function zmq_poll_poll(resource $poll, int $timeout, mixed &$readSocketsId, mixed &$writeSocketsId, mixed &$errorSocketsId): int;

<<__Native>>
function zmq_poll_remove(resource $poll, string $id): int;

<<__Native>>
function zmq_poll_clear(resource $poll): int;
