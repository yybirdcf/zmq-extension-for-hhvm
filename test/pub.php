<?php

$context = new ZMQContext();

//push message mode
$sender = new ZMQSocket($context, ZMQ::SOCKET_PUB, 'socket#1');
$sender->bind('tcp://127.0.0.1:5555');

while (true) {
    // Get values that will fool the boss
    $zipcode = mt_rand(0, 100000);
    $temperature = mt_rand(-80, 135);
    $relhumidity = mt_rand(10, 60);

    // Send message to all subscribers
    $update = sprintf ("%05d %d %d", $zipcode, $temperature, $relhumidity);
    echo $update . PHP_EOL;
    $sender->send($update);
    sleep(1);
}