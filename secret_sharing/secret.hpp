#include <boost/array.hpp>

#include "poly.hpp"

#ifndef SECRET_H_
#define SECRET_H_

template<class SecretT, size_t NumParties>
class Secret {
public:

  typedef SecretT value_t;
  typedef boost::array<SecretT, NumParties> shares_array_t;

  Secret(SecretT secret);

  boost::array<SecretT, NumParties> share();
  boost::array<SecretT, NumParties> share(SecretT secret);

private:

  size_t num_parties_;
  Poly poly_;
};

#endif

#include "secret_template.hpp"
