
#ifndef _SHA1_H_
#define _SHA1_H_

#include<string.h>
#include<stdio.h>

class SHA1
{
	public:
		SHA1();
		virtual ~SHA1();
		bool SHA_GO(const char *lpData_Input, char *lpSHACode_Output );
	private:
		unsigned int H[5];					// Message digest buffers
		unsigned int Length_Low;				// Message length in bits
		unsigned int Length_High;			// Message length in bits
		unsigned char Message_Block[64];	// 512-bit message blocks
		int Message_Block_Index;				// Index into message block array
	private:
		void SHAInit();
		void AddDataLen(int nDealDataLen);

		// Process the next 512 bits of the message
		void ProcessMessageBlock();

		// Pads the current message block to 512 bits
		void PadMessage();

		// Performs a circular left shift operation
		inline unsigned CircularShift(int bits, unsigned word);
};

#endif
