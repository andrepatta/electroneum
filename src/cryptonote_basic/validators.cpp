// Copyrights(c) 2017-2019, The Electroneum Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include "validators.h"

#undef ELECTRONEUM_DEFAULT_LOG_CATEGORY
#define ELECTRONEUM_DEFAULT_LOG_CATEGORY "Validators"

namespace electroneum {
    namespace basic {
        Validator::Validator(const std::string &publicKey, uint64_t startHeight, uint64_t endHeight)
                : publicKey(publicKey), startHeight(startHeight), endHeight(endHeight) {}

        Validator::Validator() = default;

        Validators::Validators() {
            this->http_client.set_server(this->endpoint_addr, this->endpoint_port, boost::none);
        };

        bool Validators::validate_and_update(electroneum::basic::v_list_struct res) {
          //Check against our hardcoded public-key to make sure it's a valid message
          if (res.public_key != "F669F5CDD45CE7C540A5E85CAB04F970A30E20D2C939FD5ACEB18280C9319C1D") {
            LOG_PRINT_L1("Validator list has invalid public_key.");
            return false;
          }

          bool is_signature_valid = crypto::verify_signature(res.blob, unhex(string(res.public_key)),
                                                             unhex(string(res.signature)));
          if (!is_signature_valid) {
            LOG_PRINT_L1("Validator list has invalid signature and will be ignored.");
            return false;
          }

          json_obj obj;
          load_t_from_json(obj, crypto::base64_decode(res.blob));
          for (const auto &v : obj.validators) {
            this->addOrUpdate(v.validation_public_key, v.start_height, v.end_height);
          }

          //Serialize & save valid http response to propagate to p2p uppon request
          this->serialized_v_list = store_t_to_json(res);
          this->last_updated = time(nullptr);
          this->status = ValidatorsState::Valid;

          MGINFO_MAGENTA("Validators list successfully updated!");
          return true;
        }

        void Validators::add(const string &key, uint64_t startHeight, uint64_t endHeight) {
          if (!this->exists(key)) this->list.emplace_back(new Validator(key, startHeight, endHeight));
        }

        void Validators::addOrUpdate(const string &key, uint64_t startHeight, uint64_t endHeight) {
          this->exists(key) ? this->update(key, endHeight) : this->list.emplace_back(
                  new Validator(key, startHeight, endHeight));
        }

        Validator* Validators::find(const string &key) {
          auto it = find_if(this->list.begin(), this->list.end(), [&key](Validator *&v) {
              return v->getPublicKey() == key;
          });
          return *it;
        }

        bool Validators::exists(const string &key) {
          bool found = false;
          all_of(this->list.begin(), this->list.end(), [&key, &found](Validator *&v) {
              if (v->getPublicKey() == key) {
                found = true;
                return false;
              }
              return true;
          });
          return found;
        }

        void Validators::update(const string &key, uint64_t endHeight) {
          find_if(this->list.begin(), this->list.end(), [&key, &endHeight](Validator *&v) {
              if (v->getPublicKey() == key) {
                v->setEndHeight(endHeight);
                return true;
              }
              return false;
          });
        }

        void Validators::invalidate() {
          this->serialized_v_list = string("");
          this->status = ValidatorsState::Invalid;
        }

        ValidatorsState Validators::validate_expiration() {
          if(this->status == ValidatorsState::Valid && (time(nullptr) - this->last_updated) >= this->timeout) {
            this->serialized_v_list = string("");
            this->status = ValidatorsState::Expired;
          }

          return this->status;
        }
    }
}