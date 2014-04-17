<?php

$context = new ZMQContext();

$frontend = new ZMQSocket($context, ZMQ::SOCKET_SUB);
$backend = new ZMQSocket($context, ZMQ::SOCKET_SUB);
$frontend->connect("tcp://127.0.0.1:5555");
$backend->connect("tcp://127.0.0.1:5555");
$frontend->setSockOpt(ZMQ::SOCKOPT_LINGER, 0);
$backend->setSockOpt(ZMQ::SOCKOPT_LINGER, 0);
$frontend->setSockOpt(ZMQ::SOCKOPT_SUBSCRIBE, '2');
$backend->setSockOpt(ZMQ::SOCKOPT_SUBSCRIBE, '4');

$poll = new ZMQPoll();
$poll->add($frontend, ZMQ::POLL_IN);
$poll->add($backend, ZMQ::POLL_IN);

$readable = $writeable = array();

while(true) {
    $events = $poll->poll($readable, $writeable, 25000);
    echo $events . PHP_EOL;
    foreach($readable as $socket) {
        $message = $socket->recv();
        echo $message . PHP_EOL;
    }
}