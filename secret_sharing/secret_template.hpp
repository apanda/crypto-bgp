/*
 * secret_template.hpp
 *
 *  Created on: Oct 25, 2012
 *      Author: vjeko
 */

#ifndef SECRET_TEMPLATE_HPP_
#define SECRET_TEMPLATE_HPP_

#include <iostream>
#include <boost/assert.hpp>

#include <common.hpp>

template <typename SecretT, size_t NumParties>
Secret<SecretT, NumParties>::Secret(SecretT secret) :
    num_parties_(NumParties),  poly_(NumParties - 1, secret) {

  BOOST_ASSERT_MSG( NumParties > 1, "The minimum number of parties is two." );

}


template <typename SecretT, size_t NumParties>
boost::array<SecretT, NumParties>
Secret<SecretT, NumParties>::share() {

  boost::array<SecretT, NumParties> shares;

  for(size_t i = 0; i < NumParties; i++) {
    shares[i] = poly_.eval(i + 1);
  }

  return shares;
}


template <typename SecretT, size_t NumParties>
boost::array<SecretT, NumParties>
Secret<SecretT, NumParties>::share(SecretT secret) {

  poly_.set_y0(secret);
  return share();

}


#endif /* SECRET_TEMPLATE_HPP_ */
