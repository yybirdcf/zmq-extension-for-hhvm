<?php

$context = new ZMQContext();

$receiver = new ZMQSocket($context, ZMQ::SOCKET_SUB, 'socket#2');
$receiver->connect('tcp://127.0.0.1:5555');
$receiver->setSockOpt(ZMQ::SOCKOPT_SUBSCRIBE, '2');
while(true)
{
    $msg = $receiver->recv();
    echo $msg . PHP_EOL;
}