<?php

class UnitTest extends PHPUnit_Framework_TestCase
{

    public function testContext()
    {
		$this->assertTrue(class_exists("ZMQ"));
        $context = new ZMQContext();
        $this->assertTrue($context->isPersistent());

        $context->setOpt(ZMQ::ZMQ_IO_THREADS, 3);
        $context->setOpt(ZMQ::ZMQ_MAX_SOCKETS, 3);

        $this->assertEquals(3, $context->getOpt(ZMQ::ZMQ_IO_THREADS));
        $this->assertEquals(3, $context->getOpt(ZMQ::ZMQ_MAX_SOCKETS));

        $context = new ZMQContext(2);

        $this->assertEquals(2, $context->getOpt(ZMQ::ZMQ_IO_THREADS));
    }

    public function testSocket()
    {
        $context = new ZMQContext();

        $socket = new ZMQSocket($context, ZMQ::SOCKET_PUSH, 'socket # push');
        $this->assertEquals(ZMQ::SOCKET_PUSH, $socket->getSocketType());
        $this->assertTrue($socket->isPersistent());
        $this->assertEquals('socket # push', $socket->getPersistentId());
    }

    public function testPoll()
    {
        $context = new ZMQContext();

        $frontend = new ZMQSocket($context, ZMQ::SOCKET_ROUTER);
        $backend = new ZMQSocket($context, ZMQ::SOCKET_DEALER);
        $frontend->bind("tcp://*:5559");
        $backend->bind("tcp://*:5560");

        $poll = new ZMQPoll();
        $poll->add($frontend, ZMQ::POLL_IN);
        $poll->add($backend, ZMQ::POLL_IN);

        $this->assertEquals(2, $poll->count());
        $poll->remove($frontend);
        $this->assertEquals(1, $poll->count());
        $poll->clear();
        $this->assertEquals(0, $poll->count());
    }
}
