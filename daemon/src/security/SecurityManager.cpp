#include "security/SecurityManager.h"
#include "security/SecurityKeyManager.h"
#include "core/BundleCore.h"
#include "routing/QueueBundleEvent.h"
#include <ibrcommon/Logger.h>

#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/err.h>

#ifdef __DEVELOPMENT_ASSERTIONS__
#include <cassert>
#endif

namespace dtn
{
	namespace security
	{
		SecurityManager& SecurityManager::getInstance()
		{
			static SecurityManager sec_man;
			return sec_man;
		}

		SecurityManager::SecurityManager()
		: _accept_only_bab(false), _accept_only_pib(false), _pib_node_key_send(0), _pcb_node_key_recv(0), _esb_node_key_recv(0), _pib_public_key(0), _pcb_public_key(0), _esb_public_key(0)
		{
		}

		SecurityManager::~SecurityManager()
		{
			clearKeys();
		}

		void SecurityManager::sign(dtn::data::Bundle &bundle) const throw (KeyMissingException)
		{
			// TODO: implement sign process
			IBRCOMMON_LOGGER_DEBUG(10) << "sign bundle: " << bundle.toString() << IBRCOMMON_LOGGER_ENDL;
		}

		void SecurityManager::prefetchKey(const dtn::data::EID &eid)
		{
			// TODO: implement key prefetch procedure
			IBRCOMMON_LOGGER_DEBUG(10) << "prefetch key for: " << eid.getString() << IBRCOMMON_LOGGER_ENDL;
		}

		void SecurityManager::verify(dtn::data::Bundle &bundle) const throw (VerificationFailedException)
		{
			// TODO: remove signature blocks, if possible
			IBRCOMMON_LOGGER_DEBUG(10) << "verify bundle: " << bundle.toString() << IBRCOMMON_LOGGER_ENDL;

			// set the verify bit, after verification
			bundle.set(dtn::data::Bundle::DTNSEC_STATUS_VERIFIED, true);
		}

		void SecurityManager::fastverify(const dtn::data::Bundle &bundle) const throw (VerificationFailedException)
		{
			// TODO: do a fast verify without manipulating the bundle
			const dtn::daemon::Configuration::Security &secconf = dtn::daemon::Configuration::getInstance().getSecurity();

			if (secconf.getLevel() & dtn::daemon::Configuration::Security::SECURITY_LEVEL_ENCRYPTED)
			{
				// check if the bundle is encrypted and throw an exception if not
				//throw VerificationFailedException("Bundle is not encrypted");
				IBRCOMMON_LOGGER_DEBUG(10) << "encryption required, verify bundle: " << bundle.toString() << IBRCOMMON_LOGGER_ENDL;
			}

			if (secconf.getLevel() & dtn::daemon::Configuration::Security::SECURITY_LEVEL_SIGNED)
			{
				// check if the bundle is signed and throw an exception if not
				//throw VerificationFailedException("Bundle is not signed");
				IBRCOMMON_LOGGER_DEBUG(10) << "signature required, verify bundle: " << bundle.toString() << IBRCOMMON_LOGGER_ENDL;
			}
		}

		void SecurityManager::decrypt(dtn::data::Bundle &bundle) const throw (DecryptException, KeyMissingException)
		{
			// TODO: check if the bundle has to be decrypted, return when not

			// TODO: decrypt

			IBRCOMMON_LOGGER_DEBUG(10) << "decrypt bundle: " << bundle.toString() << IBRCOMMON_LOGGER_ENDL;

			bundle.set(dtn::data::Bundle::DTNSEC_STATUS_CONFIDENTIAL, true);
		}

		void SecurityManager::encrypt(dtn::data::Bundle &bundle) const throw (EncryptException, KeyMissingException)
		{
			// TODO: encrypt the bundle

			IBRCOMMON_LOGGER_DEBUG(10) << "encrypt bundle: " << bundle.toString() << IBRCOMMON_LOGGER_ENDL;
		}

		void SecurityManager::deleteKey(const dtn::data::EID& node, SecurityBlock::BLOCK_TYPES bt)
		{
			std::map <dtn::data::EID, RSA* > * map = 0;
			switch (bt)
			{
				case SecurityBlock::BUNDLE_AUTHENTICATION_BLOCK:
					_bab_node_key.erase(node.getNodeEID());
					break;
				case SecurityBlock::PAYLOAD_INTEGRITY_BLOCK:
					map = &_pib_node_key_recv;
					break;
				case SecurityBlock::PAYLOAD_CONFIDENTIAL_BLOCK:
					map = &_pcb_node_key_send;
					break;
				case SecurityBlock::EXTENSION_SECURITY_BLOCK:
					map = &_esb_node_key_send;
					break;
			}
			if (map)
			{
				// if not copied operator[] from map will fail, dont know why
				dtn::data::EID node1(node.getNodeEID());
				size_t map_size = map->size();
				RSA * rsa = map->operator[](node1);
				if (rsa)
				{
					RSA_free(map->operator[](node1));
					map->operator[](node1) = NULL;
				}
				map_size = map->size();
				map->erase(node1);
			}
		}

//		RSA * SecurityManager::loadKey(std::map <dtn::data::EID, RSA* >& map, const dtn::data::EID& eid, SecurityBlock::BLOCK_TYPES bt)
//		{
//			RSA * rsa = 0;
//			std::map <dtn::data::EID, RSA* >::iterator it = map.find(eid.getNodeEID());
//			if (it == map.end())
//			{
//				std::string key = dtn::daemon::Configuration::getInstance().getSecurity().getPublicKey(eid.getNodeEID(), bt);
//				if (key.size() > 0)
//				{
//					SecurityManager::read_public_key(key, &rsa);
//					map[eid.getNodeEID()] = rsa;
//				}
//			}
//			else
//				rsa = it->second;
//			return rsa;
//		}

//		RSA * SecurityManager::loadKey_public(RSA ** rsa, SecurityBlock::BLOCK_TYPES bt)
//		{
//			if (*rsa == NULL)
//			{
//				std::pair<std::string, std::string> keys = dtn::daemon::Configuration::getInstance().getSecurity().getPrivateAndPublicKey(bt);
//				if (keys.first.size() > 0)
//					read_public_key(keys.second, rsa);
//			}
//
//			return *rsa;
//		}
//
//		RSA* SecurityManager::loadKey(RSA ** rsa, SecurityBlock::BLOCK_TYPES bt)
//		{
//			if (*rsa == NULL)
//			{
//				std::pair<std::string, std::string> keys = dtn::daemon::Configuration::getInstance().getSecurity().getPrivateAndPublicKey(bt);
//				if (keys.first.size() > 0)
//					read_private_key(keys.first, rsa);
//			}
//
//			return *rsa;
//		}

		std::string SecurityManager::readSymmetricKey(const std::string& file)
		{
			std::string key("");
			std::ifstream ifs;
			ifs.open(file.c_str());
			if (!ifs)
				return key;

			//this should atm be big enough
			size_t huge = 1000;
			char * temp_key = new char[huge];

			ifs.read(temp_key, huge);

#ifdef __DEVELOPMENT_ASSERTIONS__
			assert(ifs.eof());
#endif

			key = std::string(temp_key, ifs.gcount()-1);
			ifs.close();

			delete temp_key;

			return key;
		}

		std::string SecurityManager::loadKey(std::map<dtn::data::EID, std::string>& map, const dtn::data::EID& node, SecurityBlock::BLOCK_TYPES bt)
		{
//			std::string key("");
//			std::map<dtn::data::EID, std::string>::iterator it = map.find(node.getNodeEID());
//			if (it == map.end())
//			{
//				std::string keypath = dtn::daemon::Configuration::getInstance().getSecurity().getPublicKey(node.getNodeEID(), bt);
//				if (keypath.size() == 0)
//					return key;
//				key = readSymmetricKey(keypath);
//				map[node.getNodeEID()] = key;
//			}
//			else
//				key = it->second;
//			return key;
		}

		RSA * SecurityManager::getPublicKey(const dtn::data::EID& node, SecurityBlock::BLOCK_TYPES bt)
		{
			std::map <dtn::data::EID, RSA* > * map = 0;
			switch (bt)
			{
				case dtn::security::SecurityBlock::BUNDLE_AUTHENTICATION_BLOCK:
				{
#ifdef __DEVELOPMENT_ASSERTIONS__
					assert(false);
#endif
					break;
				}
				case dtn::security::SecurityBlock::PAYLOAD_INTEGRITY_BLOCK:
				{
					map = &_pib_node_key_recv;
					break;
				}
				case dtn::security::SecurityBlock::PAYLOAD_CONFIDENTIAL_BLOCK:
				{
					map = &_pcb_node_key_send;
					break;
				}
				case dtn::security::SecurityBlock::EXTENSION_SECURITY_BLOCK:
				{
					map = &_esb_node_key_send;
					break;
				}
			}
			//return loadKey(*map, node.getNodeEID(), bt);
		}

		RSA * SecurityManager::getPublicKey(SecurityBlock::BLOCK_TYPES bt)
		{
			RSA * rsa = 0;
			switch (bt)
			{
				case dtn::security::SecurityBlock::BUNDLE_AUTHENTICATION_BLOCK:
#ifdef __DEVELOPMENT_ASSERTIONS__
					assert(false);
#endif
					break;
				case dtn::security::SecurityBlock::PAYLOAD_INTEGRITY_BLOCK:
					rsa = _pib_public_key;
					break;
				case dtn::security::SecurityBlock::PAYLOAD_CONFIDENTIAL_BLOCK:
					rsa = _pcb_public_key;
					break;
				case dtn::security::SecurityBlock::EXTENSION_SECURITY_BLOCK:
					rsa = _esb_public_key;
					break;
			}

			//return loadKey_public(&rsa, bt);
		}

		RSA * SecurityManager::getPrivateKey(SecurityBlock::BLOCK_TYPES bt)
		{
			RSA * rsa = 0;
			switch (bt)
			{
				case dtn::security::SecurityBlock::BUNDLE_AUTHENTICATION_BLOCK:
				{
#ifdef __DEVELOPMENT_ASSERTIONS__
					assert(false);
#endif
					break;
				}
				case dtn::security::SecurityBlock::PAYLOAD_INTEGRITY_BLOCK:
				{
					rsa = _pib_node_key_send;
					break;
				}
				case dtn::security::SecurityBlock::PAYLOAD_CONFIDENTIAL_BLOCK:
				{
					rsa = _pcb_node_key_recv;
					break;
				}
				case dtn::security::SecurityBlock::EXTENSION_SECURITY_BLOCK:
				{
					rsa = _esb_node_key_recv;
					break;
				}
			}
			//return loadKey(&rsa, bt);
		}

		std::string SecurityManager::getSymmetricKey(const dtn::data::EID& node, SecurityBlock::BLOCK_TYPES bt)
		{
#ifdef __DEVELOPMENT_ASSERTIONS__
			assert(bt == SecurityBlock::BUNDLE_AUTHENTICATION_BLOCK);
#endif
			return loadKey(_bab_node_key, node.getNodeEID(), bt);
		}

		bool SecurityManager::isReadyToSend(const dtn::data::BundleID& bid)
		{
			dtn::core::BundleStorage& stor = dtn::core::BundleCore::getInstance().getStorage();
			dtn::data::Bundle bundle = stor.get(bid);
			return isReadyToSend(bundle);
		}

		std::list< KeyBlock > SecurityManager::getListOfNeededSymmetricKeys(const dtn::data::Bundle& bundle) const
		{
			const dtn::daemon::Configuration::Security& conf = dtn::daemon::Configuration::getInstance().getSecurity();
			std::list<KeyBlock> needed_keys;

			std::set<EID> neighbors;
			std::map<BundleID, std::set<EID> >::const_iterator it = _bundle_with_neighbors.find(BundleID(bundle));
			if (it != _bundle_with_neighbors.end())
				neighbors = it->second;

			for (std::set<EID>::const_iterator it = neighbors.begin(); it != neighbors.end(); it++)
			{
//				std::string key = conf.getPublicKey(*it, dtn::security::SecurityBlock::BUNDLE_AUTHENTICATION_BLOCK);
//				if (key == "")
//				{
//					KeyBlock kb;
//					kb.setTarget(*it);
//					kb.setSecurityBlockType(SecurityBlock::BUNDLE_AUTHENTICATION_BLOCK);
//					needed_keys.push_back(kb);
//				}
			}

			return needed_keys;
		}

		std::list< KeyBlock > SecurityManager::getListOfNeededKeys(const dtn::data::Bundle& bundle) const
		{
			// iterate over all tokens of a rule and check if all needed public keys
			// are present. if one is missing return false and request all missing
			// keys.
			std::list<KeyBlock> needed_keys;

			// asymmetric keys
			std::list<dtn::security::SecurityRule::RuleToken> rules = getRule(bundle._destination.getNodeEID()).getRules();
			for (std::list<dtn::security::SecurityRule::RuleToken>::iterator it = rules.begin(); it != rules.end(); it++)
			{
				const std::list<dtn::data::EID>& targets = it->getTargets();
				for (std::list<dtn::data::EID>::const_iterator target_it = targets.begin(); target_it != targets.end(); target_it++)
				{
//					// look if a key is present
//					if (conf.getPublicKey(*target_it, it->getType()) == "")
//					{
//						KeyBlock kb;
//						kb.setTarget(*target_it);
//						kb.setSecurityBlockType(it->getType());
//						needed_keys.push_back(kb);
//						KeyRequestEvent::raise(*target_it, it->getType());
//					}
				}
			}
			return needed_keys;
		}

		bool SecurityManager::isReadyToSend(const Bundle& bundle)
		{
//			_bundle_with_neighbors[BundleID(bundle)].insert(eid);

			std::list<KeyBlock> needed_keys = getListOfNeededKeys(bundle);

			if (!needed_keys.empty())
			{
				BundleID bid(bundle);
				_pending_keys[bid] = needed_keys;
				IBRCOMMON_LOGGER_ex(notice) << "Bundle from " << bundle._source.getString() << " is waiting for keys to arrive" << IBRCOMMON_LOGGER_ENDL;
			}

			return needed_keys.empty();
		}

		void SecurityManager::readRoutingTable()
		{
			const std::list<dtn::security::SecurityRule> rules = dtn::daemon::Configuration::getInstance().getSecurity().getSecurityRules();
			_security_rules_routing.clear();
			for (std::list<dtn::security::SecurityRule>::const_iterator it = rules.begin(); it != rules.end(); it++)
				_security_rules_routing[it->getDestination()] = *it;
		}

		int SecurityManager::read_private_key(const std::string& filename, RSA ** rsa)
		{
			FILE * rsa_pkey_file = fopen(filename.c_str(), "r");
			if (!rsa_pkey_file) {
				IBRCOMMON_LOGGER_ex(critical) << "Failed to open " << filename << IBRCOMMON_LOGGER_ENDL;
				return 2;
			}
			if (!PEM_read_RSAPrivateKey(rsa_pkey_file, rsa, NULL, NULL)) {
				IBRCOMMON_LOGGER_ex(critical) << "Error loading RSA private key file: " << filename << IBRCOMMON_LOGGER_ENDL;
				ERR_print_errors_fp(stderr);
				return 2;
			}
			fclose(rsa_pkey_file);
			return 0;
		}

		int SecurityManager::read_public_key(const std::string& filename, RSA ** rsa)
		{
			FILE * rsa_pkey_file = fopen(filename.c_str(), "r");
			if (!rsa_pkey_file) {
				IBRCOMMON_LOGGER_ex(critical) << "Failed to open " << filename << IBRCOMMON_LOGGER_ENDL;
				return 2;
			}
			if (!PEM_read_RSA_PUBKEY(rsa_pkey_file, rsa, NULL, NULL)) {
				IBRCOMMON_LOGGER_ex(critical) << "Error loading RSA public key file: " << filename << IBRCOMMON_LOGGER_ENDL;
				ERR_print_errors_fp(stderr);
				return 2;
			}
			fclose(rsa_pkey_file);
			return 0;
		}

		bool SecurityManager::incoming(dtn::data::Bundle& bundle)
		{
			bool status_ok = check_bab(bundle);
			lookForKeys(bundle);
			status_ok &= check_esb(bundle);

			std::list<const dtn::data::Block*> blocks = bundle.getBlocks();
			bool end = false;
			for (std::list<const dtn::data::Block*>::const_iterator it = blocks.begin(); it != blocks.end() && !end && status_ok;)
			{
#ifdef __DEVELOPMENT_ASSERTIONS__
				assert(*it != NULL);
#endif
				const dtn::data::Block& block = **it;
				switch (block.getType())
				{
					case SecurityBlock::BUNDLE_AUTHENTICATION_BLOCK:
						status_ok = false;
					case SecurityBlock::PAYLOAD_CONFIDENTIAL_BLOCK:
					{
						const dtn::security::PayloadConfidentialBlock& pcb = dynamic_cast<const dtn::security::PayloadConfidentialBlock&>(block);
						status_ok = check_pcb(bundle, pcb);
						break;
					}
					case SecurityBlock::PAYLOAD_INTEGRITY_BLOCK:
					{
						const dtn::security::PayloadIntegrityBlock& pib = dynamic_cast<const dtn::security::PayloadIntegrityBlock&>(block);
						status_ok = check_pib(bundle, pib);
						break;
					}
					case SecurityBlock::EXTENSION_SECURITY_BLOCK:
					default:
					{
						end = true;
						break;
					}
				}
				// handled block still inside the bundle? if not start from the beginning
				bool is_in_the_bundle = false;
				std::list<const dtn::data::Block*> blocks2 = bundle.getBlocks();
				for (std::list<const dtn::data::Block*>::const_iterator iit = blocks2.begin(); iit != blocks2.end() && !is_in_the_bundle; iit++)
					is_in_the_bundle = *it == *iit;
				if (!is_in_the_bundle)
				{
					blocks = bundle.getBlocks();
					it = blocks.begin();
				}
				else
				{
					it++;
				}
			}

			return status_ok;
		}

		bool SecurityManager::check_bab(dtn::data::Bundle& bundle)
		{
			bool status_ok = false;

			std::list <const dtn::security::BundleAuthenticationBlock* > babs = bundle.getBlocks<dtn::security::BundleAuthenticationBlock>();

			if (babs.size() == 0 && !_accept_only_bab)
				return true;

			for (std::list <const dtn::security::BundleAuthenticationBlock* >::iterator it = babs.begin(); it != babs.end() && !status_ok; it++)
			{
				const dtn::security::BundleAuthenticationBlock& bab = **it;
				// look for the right BAB-factory
				dtn::data::EID node = dtn::security::SecurityBlock::getSecuritySource(bundle, bab);
				std::string key = getSymmetricKey(node, dtn::security::SecurityBlock::BUNDLE_AUTHENTICATION_BLOCK);

				// no node found
				if (key == "")
					continue;

				dtn::security::BundleAuthenticationBlock bab_verify(reinterpret_cast<unsigned char const *>(key.c_str()), key.size(), dtn::core::BundleCore::local, node);
				status_ok = bab_verify.verify(bundle) == 1;
				if (status_ok)
					bab_verify.removeAllBundleAuthenticationBlocks(bundle);
			}

			return status_ok;
		}

		bool SecurityManager::check_pcb(dtn::data::Bundle& bundle, const dtn::security::PayloadConfidentialBlock& pcb)
		{
			bool status_ok = true;
			// look for the right PCB-factory
			if (contains_block_for_us_destination<dtn::security::PayloadConfidentialBlock>(bundle))
			{
#ifdef __DEVELOPMENT_ASSERTIONS__
				assert(getPrivateKey(dtn::security::SecurityBlock::PAYLOAD_CONFIDENTIAL_BLOCK) != 0);
#endif
				dtn::security::PayloadConfidentialBlock pcb_decrypt(getPrivateKey(dtn::security::SecurityBlock::PAYLOAD_CONFIDENTIAL_BLOCK), dtn::core::BundleCore::local);
				status_ok = pcb_decrypt.decrypt(bundle);
			}
			if (status_ok)
				IBRCOMMON_LOGGER_ex(notice) << "PayloadBlock from " << bundle._source.getString() << " successfully decrypted using PayloadConfidentialBlock" << IBRCOMMON_LOGGER_ENDL;
			return status_ok;
		}

		bool SecurityManager::check_pib(dtn::data::Bundle& bundle, const dtn::security::PayloadIntegrityBlock& pib)
		{
			// take every pib and look if we have a public key, the block could have
			// the needed keys attached to it and the KeyServerWorker will store them
			bool status_ok = true;
			bool verified = false;

			std::list<dtn::security::PayloadIntegrityBlock const *> pibs = bundle.getBlocks<dtn::security::PayloadIntegrityBlock>();
			for (std::list<dtn::security::PayloadIntegrityBlock const *>::iterator it = pibs.begin(); it != pibs.end() && status_ok; it++)
			{
				RSA * rsa = getPublicKey(dtn::security::SecurityBlock::getSecuritySource(bundle, **it), dtn::security::SecurityBlock::PAYLOAD_INTEGRITY_BLOCK);
				if (rsa != 0)
				{
					dtn::security::PayloadIntegrityBlock pib_verify(rsa, dtn::core::BundleCore::local, dtn::security::SecurityBlock::getSecuritySource(bundle, **it));
					if (dtn::security::SecurityBlock::isSecurityDestination(bundle, **it, dtn::core::BundleCore::local))
						status_ok = (pib_verify.verifyAndRemoveMatchingBlock(bundle) > 0);
					else
					{
						if (!verified)
							status_ok = verified = pib_verify.verify(bundle);
					}
					if (status_ok)
						IBRCOMMON_LOGGER_ex(notice) << "Bundle from " << bundle._source.getString() << " successfully verified using PayloadIntegrityBlock" << IBRCOMMON_LOGGER_ENDL;
				}
				else
				{
					if (dtn::security::SecurityBlock::isSecurityDestination(bundle, **it, dtn::core::BundleCore::local))
						status_ok = false;
				}
			}

			return status_ok;
		}

		bool SecurityManager::check_esb(Bundle& bundle)
		{
			bool status_ok = true;

			if (contains_block_for_us_destination<ExtensionSecurityBlock>(bundle))
			{
#ifdef __DEVELOPMENT_ASSERTIONS__
				assert(getPrivateKey(dtn::security::SecurityBlock::EXTENSION_SECURITY_BLOCK) != 0);
#endif
				dtn::security::ExtensionSecurityBlock esb_decrypt(getPrivateKey(dtn::security::SecurityBlock::EXTENSION_SECURITY_BLOCK), dtn::core::BundleCore::local);
				status_ok = esb_decrypt.decryptBlock(bundle);
				if (status_ok)
					IBRCOMMON_LOGGER_ex(notice) << "Block from " << bundle._source.getString() << " successfully decrypted using ExtensionSecurityBlock" << IBRCOMMON_LOGGER_ENDL;
			}

			return status_ok;
		}

		void SecurityManager::addKeysForVerfication(Bundle& bundle)
		{
			dtn::security::KeyBlock& kb = bundle.push_back<dtn::security::KeyBlock>();
			kb.addKey(getPublicKey(SecurityBlock::PAYLOAD_INTEGRITY_BLOCK));
			kb.setTarget(dtn::core::BundleCore::local);
		}

		int SecurityManager::outgoing_asymmetric(dtn::data::Bundle& bundle, const dtn::data::EID& origin)
		{
			_bundle_with_origin[BundleID(bundle)] = origin;
			return outgoing_asymmetric(bundle);
		}

		int SecurityManager::outgoing_asymmetric(dtn::data::BundleID& bid)
		{
			dtn::core::BundleStorage& stor = dtn::core::BundleCore::getInstance().getStorage();
			dtn::data::Bundle bundle = stor.get(bid);
			return outgoing_asymmetric(bundle);
		}

		int SecurityManager::outgoing_asymmetric(dtn::data::Bundle& bundle)
		{
			// for /keyserver no rules apply, except the BundleAuthenticationBlocks
			bool is_keyserver = bundle._destination.getString().rfind(KeyServerWorker::getSuffix()) != std::string::npos;

			// check if the bundle can be send
			if (!is_keyserver && !isReadyToSend(bundle))
				return 1;
			// need information about the route
			// knows next hop and the destination, needs to be enough
			// 1. PCB/PIB/ESB

			dtn::security::SecurityRule rule = getRule(bundle._destination.getNodeEID());
			const std::list<dtn::security::SecurityRule::RuleToken>& rules = rule.getRules();
			for (std::list<dtn::security::SecurityRule::RuleToken>::const_iterator it = rules.begin(); it != rules.end() && !is_keyserver; it++)
			{
				switch (it->getType())
				{
					case SecurityBlock::PAYLOAD_INTEGRITY_BLOCK:
					{
#ifdef __DEVELOPMENT_ASSERTIONS__
						assert(getPrivateKey(it->getType()) != 0);
#endif
						IBRCOMMON_LOGGER_ex(notice) << "Adding a PayloadIntegrityBlock to a bundle with destination " << bundle._destination.getString() << IBRCOMMON_LOGGER_ENDL;
						dtn::security::PayloadIntegrityBlock pib(getPrivateKey(it->getType()), dtn::core::BundleCore::local);
						pib.addHash(bundle);
						addKeyBlocksForPIB(bundle);
						break;
					}
					case SecurityBlock::PAYLOAD_CONFIDENTIAL_BLOCK:
					{
						IBRCOMMON_LOGGER_ex(notice) << "Adding a PayloadConfidentialBlock to a bundle with destination " << bundle._destination.getString() << IBRCOMMON_LOGGER_ENDL;

						std::list<dtn::data::EID >::const_iterator target_it = it->getTargets().begin();

						RSA * rsa = getPublicKey(*target_it, it->getType());

						// here would be the place to add more keys to the PCB
						dtn::security::PayloadConfidentialBlock pcb(rsa, dtn::core::BundleCore::local, *target_it);
						target_it++;
						for (; target_it != it->getTargets().end(); target_it++)
						{
							rsa = getPublicKey(*target_it, it->getType());
							pcb.addDestination(rsa, *target_it);
						}

						pcb.encrypt(bundle);
						break;
					}
				}
			}

			if (!is_keyserver && !rules.empty())
				try
				{
					dtn::core::BundleStorage& stor = dtn::core::BundleCore::getInstance().getStorage();
					stor.remove(bundle);
					stor.store(bundle);
				}
				catch (...) {}

			// raise the queued event to notify all receivers about the new bundle
			BundleID bid(bundle);
			dtn::routing::QueueBundleEvent::raise(MetaBundle(bundle), _bundle_with_origin[bid]);
			_bundle_with_origin.erase(bid);

			return 0;
		}

		int SecurityManager::outgoing_p2p(const dtn::data::EID next_hop, Bundle& bundle)
		{
			// 2. BAB
			std::string key = getSymmetricKey(next_hop, dtn::security::SecurityBlock::BUNDLE_AUTHENTICATION_BLOCK);
			if (key.size() > 0)
			{
				dtn::security::BundleAuthenticationBlock bab(reinterpret_cast<unsigned char const *>(key.c_str()), key.size(), dtn::core::BundleCore::local, next_hop);
				bab.addMAC(bundle);
			}
			return 0;
		}

		void delete_rsa(std::map<dtn::data::EID, RSA *>& eid_rsa)
		{
			for (std::map<dtn::data::EID, RSA *>::iterator it = eid_rsa.begin(); it != eid_rsa.end(); it++)
			{
				RSA_free(it->second);
				it->second = NULL;
			}
			eid_rsa.clear();
		}

		void SecurityManager::clearKeys()
		{
			// bab
			_bab_node_key.clear();

			// private keys
			RSA_free(_pib_node_key_send);
			RSA_free(_pcb_node_key_recv);
			RSA_free(_esb_node_key_recv);

			// public keys
			delete_rsa(_pib_node_key_recv);
			delete_rsa(_pcb_node_key_send);
			delete_rsa(_esb_node_key_send);
		}

		void SecurityManager::lookForKeys(const dtn::data::Bundle& bundle)
		{
			std::list<const dtn::security::KeyBlock*> blocks = bundle.getBlocks<dtn::security::KeyBlock>();
			for (std::list<const dtn::security::KeyBlock*>::iterator it = blocks.begin(); it != blocks.end(); it++)
			{
				const dtn::security::KeyBlock &keyblock = *(*it);

				// carries the block a key?
				if (keyblock.getKey() == "")
					continue;

				// look if the key is already there
				if (SecurityKeyManager::getInstance().hasKey(keyblock.getTarget(), SecurityKeyManager::KEY_PUBLIC))
				{
					// should we update the key?
					continue;
				}

				// store the key
				SecurityKeyManager::getInstance().store(keyblock.getTarget(), keyblock.getKey(), SecurityKeyManager::KEY_PUBLIC);

				// and call newKeyReceived
				newKeyReceived((*it)->getTarget(), (*it)->getSecurityBlockType());
			}
		}

		void SecurityManager::newKeyReceived(const dtn::data::EID& node, dtn::security::SecurityBlock::BLOCK_TYPES bt)
		{
			IBRCOMMON_LOGGER_ex(notice) << "Got new key for EID " << node.getNodeEID() << " and Securityblocktype " << bt << IBRCOMMON_LOGGER_ENDL;
			// find all bundles, which got all keys they need
			std::list<dtn::data::BundleID> sendable_blocks;
			for (std::map<dtn::data::BundleID, std::list<dtn::security::KeyBlock> >::iterator it  = _pending_keys.begin(); it != _pending_keys.end(); it++)
			{
				std::list<dtn::security::KeyBlock>::const_iterator cit = getMissingKey(node, bt, it->second);
				if (cit != it->second.end())
					it->second.remove(*cit);
				if (it->second.empty())
					sendable_blocks.push_back(it->first);
			}

			// these bundles have all keys they need so they can finally arrive
			for (std::list<dtn::data::BundleID>::iterator it = sendable_blocks.begin(); it != sendable_blocks.end(); it++)
			{
				_pending_keys.erase(*it);
				outgoing_asymmetric(*it);
			}
		}

		std::list<dtn::security::KeyBlock>::const_iterator SecurityManager::getMissingKey(const dtn::data::EID& node, dtn::security::SecurityBlock::BLOCK_TYPES bt, const std::list<dtn::security::KeyBlock>& kbs) const
		{
			std::list<dtn::security::KeyBlock>::const_iterator it = kbs.begin();
			while (it != kbs.end())
				if (it->getTarget() == node && it->getSecurityBlockType() == bt)
					break;
			return it;
		}

		void SecurityManager::addKeyBlocksForPIB(dtn::data::Bundle& bundle)
		{
			// check if the keyblock is already present
			std::list <const dtn::security::KeyBlock* > kbs = bundle.getBlocks<dtn::security::KeyBlock>();
			for (std::list <const dtn::security::KeyBlock* >::iterator it = kbs.begin(); it != kbs.end(); it++)
			{
				dtn::security::KeyBlock const * kb = *it;
				if (kb->getSecurityBlockType() == SecurityBlock::PAYLOAD_INTEGRITY_BLOCK && kb->getTarget() == dtn::core::BundleCore::local)
					bundle.remove(*kb);
			}

			// TODO add later the ca and sub-ca
			dtn::security::KeyBlock& kb = bundle.push_back<dtn::security::KeyBlock>();
			kb.setSecurityBlockType(SecurityBlock::PAYLOAD_INTEGRITY_BLOCK);
			kb.setTarget(dtn::core::BundleCore::local);
			kb.addKey(getPublicKey(SecurityBlock::PAYLOAD_INTEGRITY_BLOCK));
		}

		dtn::security::SecurityRule SecurityManager::getRule(const dtn::data::EID& eid) const
		{
			// TODO asyncronous loading
			// at the moment we do nothing complex, just return the static rule from
			// the configuration
			// longest prefix matching would be possible
			dtn::security::SecurityRule the_rule;
			std::map<dtn::data::EID, dtn::security::SecurityRule>::const_iterator rule = _security_rules_routing.find(eid.getNodeEID());
			if (rule != _security_rules_routing.end())
				the_rule = rule->second;

			return the_rule;
		}
	}
}
