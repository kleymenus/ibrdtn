/*
 * lowpanstream.h
 *
 *  Created on: 29.07.2009
 *      Author: morgenro
 */

#ifndef IBRCOMMON_TCPSTREAM_H_
#define IBRCOMMON_TCPSTREAM_H_

#include "ibrcommon/Exceptions.h"
#include <streambuf>
#include <sys/types.h>
#include <sys/socket.h>
#include <iostream>
#include "ibrcommon/net/vsocket.h"
#include "ibrcommon/thread/Conditional.h"

namespace ibrcommon
{
	class lowpanstream_callback
	{
	public:
		virtual void send(char *buf, int len, unsigned int address) = 0;
	};

	class lowpanstream : public std::basic_streambuf<char, std::char_traits<char> >, public std::iostream
	{
	public:
		// The size of the input and output buffers.
		static const size_t BUFF_SIZE = 115;

		lowpanstream(lowpanstream_callback &callback, unsigned int address);
		virtual ~lowpanstream();

		void queue(char *buf, int len);

		void close();

	protected:
		virtual int sync();
		virtual int overflow(int = std::char_traits<char>::eof());
		virtual int underflow();

		unsigned int _address;

	private:
		// Input buffer
		char *in_buf_;
		bool in_buf_free;

		// Output buffer
		char *out_buf_;

		ibrcommon::Conditional in_buf_cond;
	};
}

#endif /* TCPSTREAM_H_ */