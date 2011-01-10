#ifndef SECURITYBLOCK_H_
#define SECURITYBLOCK_H_

#include "ibrdtn/data/Block.h"
#include "ibrdtn/data/EID.h"
#include "ibrdtn/data/BundleString.h"
#include "ibrdtn/data/Bundle.h"
#include <ibrcommon/ssl/AES128Stream.h> // TODO <-- this include sucks
#include <list>
#include <sys/types.h>

// forward deklaration needed for struct RSA
struct rsa_st;
// forward deklaration of RSA object
typedef rsa_st RSA;

namespace dtn
{
	namespace security
	{
		/**
		Abstract base class for BundleAuthenticationBlock, PayloadIntegrityBlock,
		PayloadConfidentialBlock and ExtensionSecurityBlock. It provides definitions
		of constants and basic functions, which are shared among the blocks.
		These blocks can be serialized in three forms. In normal form, which is 
		needed for transmission, in strict canonical form, which is need for the 
		BundleAuthenticationBlock and in mutable canonical form. In strict canonical
		form the security result can be set to be ignored. In mutable canonical form
		all SDNVs are unpacked to 8 byte fields, numbers written in network byte
		order and even the security result may be ignored, too. Because the size of
		security result of the children cannot be known in advance, the children 
		have to implement a method for getting this size.
		*/
		class SecurityBlock : public dtn::data::Block
		{
			friend class StrictSerializer;
			friend class MutualSerializer;
		public:
			/** the block id for each block type */
			enum BLOCK_TYPES
			{
				BUNDLE_AUTHENTICATION_BLOCK = 0x02,
				PAYLOAD_INTEGRITY_BLOCK = 0x03,
				PAYLOAD_CONFIDENTIAL_BLOCK = 0x04,
				EXTENSION_SECURITY_BLOCK = 0x09
			};
			/** the id of each TLV type, which is used in security parameters or 
			security result */
			enum TLV_TYPES
			{
				not_set = 0,
				initialization_vector = 1,
				key_information = 3,
				fragment_range = 4,
				integrity_signature = 5,
				salt = 7,
				PCB_integrity_check_value = 8,
				encapsulated_block = 10,
				block_type_of_encapsulated_block = 11
			};
			/** the position of each flag in the ciphersuite flags */
			enum CIPHERSUITE_FLAGS
			{
				CONTAINS_SECURITY_RESULT = 1 << 0,
				CONTAINS_CORRELATOR = 1 << 1,
				CONTAINS_CIPHERSUITE_PARAMS = 1 << 2,
				CONTAINS_SECURITY_DESTINATION = 1 << 3,
				CONTAINS_SECURITY_SOURCE = 1 << 4,
				BIT5_RESERVED = 1 << 5,
				BIT6_RESERVED = 1 << 6
			};
			/** the ids of the supported ciphersuites */
			enum CIPHERSUITE_IDS
			{
				BAB_HMAC = 0x001,
				PIB_RSA_SHA256 = 0x002,
				PCB_RSA_AES128_PAYLOAD_PIB_PCB = 0x003,
				ESB_RSA_AES128_EXT = 0x004
			};

			class TLV
			{
			public:
				TLV() : _type(not_set) {};
				TLV(TLV_TYPES type, std::string value)
				 : _type(type), _value(value)
				{ }

				bool operator<(const TLV &tlv) const;
				bool operator==(const TLV &tlv) const;

				const std::string getValue() const;
				TLV_TYPES getType() const;
				size_t getLength() const;

				friend std::ostream& operator<<(std::ostream &stream, const TLV &tlv);
				friend std::istream& operator>>(std::istream &stream, TLV &tlv);

			private:
				TLV_TYPES _type;
				dtn::data::BundleString _value;
			};

			class TLVList : public std::set<TLV>
			{
			public:
				TLVList() {};
				virtual ~TLVList() {};

				friend std::ostream& operator<<(std::ostream &stream, const TLVList &tlvlist);
				friend std::istream& operator>>(std::istream &stream, TLVList &tlvlist);

				const std::string get(TLV_TYPES type) const;
				void add(TLV_TYPES type, std::string value);
				void remove(TLV_TYPES type);

				const std::string toString() const;
				size_t getLength() const;

			private:
				size_t getPayloadLength() const;
			};

			/** does nothing */
			virtual ~SecurityBlock() = 0;

			/**
			Returns the length of this Block
			@return the length
			*/
			virtual size_t getLength() const;

			/**
			Returns the length of this Block if it would serialized in mutable
			canonical form
			@return the length in mutable canonical form
			*/
			virtual size_t getLength_mutable() const;

			/**
			Serializes this Block into stream
			@param the stream in which should be written
			@return the same stream as the input stream
			*/
			virtual std::ostream &serialize(std::ostream &stream) const;

			/**
			By default this function does nothing else than serialize(std::ostream&).
			Children have to implement it in their own way if different treatment for
			serialization is needed.
			@param stream the stream in which should be written
			@param mutual if true the mode is mutual
			@return the same stream as the input stream
			*/
			virtual std::ostream& serialize(std::ostream& stream, const bool mutual) const;

			/**
			Parses the SecurityBlock from a Stream
			@param stream the stream to read from
			*/
			virtual std::istream &deserialize(std::istream &stream);

			/**
			Returns the Security source of a SecurityBlock or dtn:none if none exists
			*/
			const dtn::data::EID getSecuritySource() const;

			/**
			Returns the Security destination of a SecurityBlock or dtn:none if none
			exists
			*/
			const dtn::data::EID getSecurityDestination() const;

			/**
			Sets the security source of this block
			@param source the security source
			*/
			void setSecuritySource(const dtn::data::EID &source);

			/**
			Sets the security destination of this block
			@param destination the security destination
			*/
			void setSecurityDestination(const dtn::data::EID &destination);
				
			/**
			Checks if the given EID is a security source for the given block
			@param bundle the bundle to which the block belongs to
			@param eid the eid of the source
			@return true if eid is the security source, false if not
			*/
			bool isSecuritySource(const dtn::data::Bundle&, const dtn::data::EID&) const;
			
			/**
			Checks if the given EID is a security destination for the given block
			@param bundle the bundle to which the block belongs to
			@param eid the eid of the destination
			@return true if eid is the security destination, false if not
			*/
			bool isSecurityDestination(const dtn::data::Bundle&, const dtn::data::EID&) const;
			
			/**
			Returns the security source of a given block
			@param bundle the bundle to which the block belongs to
			@return the security source
			*/
			const dtn::data::EID getSecuritySource(const dtn::data::Bundle&) const;
			
			/**
			Returns the security destination of a given block
			@param bundle the bundle to which the block belongs to
			@return the security destination
			*/
			const dtn::data::EID getSecurityDestination(const dtn::data::Bundle&) const;

			protected:
				/** the ciphersuite id tells what type of encryption, signature or MAC 
				is used */
				u_int64_t _ciphersuite_id;
				/** the ciphersuite flags tell if security result or parameters are 
				used, if the security destination or source is set and if a correlator 
				is used */
				u_int64_t _ciphersuite_flags;
				/** a correlator binds several security blocks in a bundle together */
				u_int64_t _correlator;

				/** you can find e.g. key information, tags, salts, 
				initialization_vectors stored als TLVs here */
				TLVList _ciphersuite_params;

				/** you can find encrypted blocks, signatures or MACs here */
				TLVList _security_result;

				/** set to true if only the length of security_result shall be written 
				into the stream */
				mutable bool ignore_security_result;

				/**
				Creates an empty SecurityBlock. This is only needed by children, which add
				blocks to bundles in a factory
				@param type type of child block
				@param id the ciphersuite
				*/
				SecurityBlock(const SecurityBlock::BLOCK_TYPES type, const CIPHERSUITE_IDS id);

				/**
				Creates a factory with a partner. If partner is empty, this must be a
				instance with a private key or a BundleAuthenticationBlock.
				@param type type of child block
				*/
				SecurityBlock(const SecurityBlock::BLOCK_TYPES type);

				/**
				Sets the ciphersuite id
				@param id ciphersuite id
				*/
				void setCiphersuiteId(const CIPHERSUITE_IDS id);

				/**
				Sets the correlator
				@param corr correlator value
				*/
				void setCorrelator(const u_int64_t corr);

				/**
				Checks if the given correlator value is used in the bundle
				@param bundle the bundle in which shall be searched for correlators
				@param correlator the correlator to be tested for uniqueness
				@return false if correlator is unique, true otherwise
				*/
				static bool isCorrelatorPresent(const dtn::data::Bundle &bundle, const u_int64_t correlator);

				/**
				Creates a unique correlatorvalue for bundle
				@param bundle the bundle for which a new unique correlator shall be 
				created
				@return a unique correlator
				*/
				static u_int64_t createCorrelatorValue(const dtn::data::Bundle &bundle);

				/**
				Tells the block if its security result shall be ommitted
				@param value if true the security_result will be ignored
				*/
				void set_ignore_security_result(const bool value) const;

				/**
				Canonicalizes the block into the stream.
				@param stream the stream to be written into
				@return the same stream as the parameter for chaining
				*/
				std::ostream &serialize_mutual(std::ostream &stream) const;

				/**
				Returns the size of the security result if it would be serialized, even 
				if it is empty. This is needed for canonicalisation. If it is empty this
				will be zero. There is no way to know how big will a payload be in 
				advance. Children have to override it for the canonicalisation forms if 
				nessessary (especial BundleAuthenticationBlock and 
				PayloadIntegrityBlock).
				@return the size of the serialized security result
				*/
				virtual size_t getSecurityResultSize() const;

				/**
				Fills salt and key with random numbers.
				@param salt reference to salt
				@param key pointer to key
				@param key_size size of key
				*/
				static void createSaltAndKey(u_int32_t& salt, unsigned char * key, size_t key_size);

				/**
				Adds a key as a TLV to a string. The key is encrypted using the public 
				key provided in the rsa object.
				@param security_parameter the string object which gets the TLV appended
				which contains the encrypted key
				@param key the plaintext key
				@param key_size the size of the plaintext key
				@param rsa object containing the public key for encryption of the
				symmetric key
				*/
				static void addKey(TLVList& security_parameter, unsigned char const * const key, size_t key_size, RSA * rsa);

				/**
				Reads a symmetric key TLV object from a string.
				@param securiy_parameter the TLVs containing string
				@param key pointer to an array to which the key will be written
				@param key_size size of the array
				@param rsa object containing the private key for decryption of the
				symmetric key
				@return true if the key has been successfully decrypted
				*/
				static bool getKey(const TLVList& security_parameter, unsigned char * key, size_t key_size, RSA * rsa);

				/**
				Adds a salt TLV object to a string.
				@param security_parameters the string
				@param salt the salt which shall be added
				*/
				static void addSalt(TLVList& security_parameters, u_int32_t salt);

				/**
				Reads a salt TLV from a string containing TLVs
				@param security_parameters string containing TLVs
				*/
				static u_int32_t getSalt(const TLVList& security_parameters);

				/**
				Copys all EIDs from one block to another and skips the first skip EIDs
				@param from source of the EIDs
				@param to destination of the EIDs
				@param skip how much EIDs should be skipped at the beginning
				*/
				static void copyEID(const dtn::data::Block& from, dtn::data::Block& to, size_t skip = 0);

				/**
				Encrypts a Block. The used initialisation vector will be written into the
				security parameters of the new SecurityBlock. The ciphertext will have the
				tag appended and be written into security result. The flags that this
				block contains ciphersuite parameters and security result will be set.
				If this is the first block, don't forget to add the key and salt to its
				security parameters.
				@param bundle the bundle which contains block
				@param block the block which shall be encrypted and encapsulated
				@param salt the salt to be used
				@param ephemeral_key the key to be used
				@return the Security Block which replaced block
				*/
				template <class T>
				static T& encryptBlock(dtn::data::Bundle& bundle, const dtn::data::Block*const block, u_int32_t salt, const unsigned char ephemeral_key[ibrcommon::AES128Stream::key_size_in_bytes]);

				/**
				Decrypts the block which is held in the SecurityBlock replaces it.
				The ciphertext is only substituted and the old block reconstructed if 
				tag verification succeeds.
				@param bundle the bundle which contains block
				@param block the security block with an encrypted block in its security
				result
				@param salt the salt
				@param ephemeral_key the key
				@return true if tag verification succeeded, false if not
				*/
				static void decryptBlock(dtn::data::Bundle& bundle, dtn::security::SecurityBlock const * block, u_int32_t salt, const unsigned char key[ibrcommon::AES128Stream::key_size_in_bytes]);

				/**
				Calculates the Size of the stream and adds a fragment range item to ciphersuite_params
				@param ciphersuite_params the string which will get a fragment range TLV added
				@param stream the stream which size will be calculated
				*/
				static void addFragmentRange(TLVList& ciphersuite_params, size_t fragmentoffset, std::istream& stream);

			private:
				/** not implemented */
				SecurityBlock(const SecurityBlock&);
				/** not implemented */
				SecurityBlock& operator=(const SecurityBlock&);
		};

		template <class T>
		T& SecurityBlock::encryptBlock(dtn::data::Bundle& bundle, const dtn::data::Block*const block, u_int32_t salt, const unsigned char ephemeral_key[ibrcommon::AES128Stream::key_size_in_bytes])
		{
			// insert ESB, block can be removed after encryption, because bundle will destroy it
			T& esb = bundle.insert<T>(*block);

			// take eid list
			copyEID(*block, esb);

			std::stringstream ss;
			ibrcommon::AES128Stream encrypt(ibrcommon::AES128Stream::ENCRYPT , ss, ephemeral_key, salt);
			dtn::data::Dictionary dict(bundle);
			dtn::data::DefaultSerializer dser(encrypt, dict);
			dser << *block;
			encrypt << std::flush;

			// append tag at the end of the ciphertext
			ss.write(reinterpret_cast<const char *>(encrypt.getTag()), ibrcommon::AES128Stream::tag_len);

			esb._security_result.add(SecurityBlock::encapsulated_block, ss.str());
			esb._ciphersuite_flags |= SecurityBlock::CONTAINS_SECURITY_RESULT;

			std::string iv(reinterpret_cast<const char *>(encrypt.getIV()), ibrcommon::AES128Stream::iv_len);
			esb._ciphersuite_params.add(SecurityBlock::initialization_vector, iv);

			esb._ciphersuite_flags |= SecurityBlock::CONTAINS_CIPHERSUITE_PARAMS;

			bundle.remove(*block);

			return esb;
		}
	}
}

#endif /* SECURITYBLOCK_H_ */
