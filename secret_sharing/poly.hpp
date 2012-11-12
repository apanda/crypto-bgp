/*
 * poly.hpp
 *
 *  Created on: Oct 25, 2012
 *      Author: vjeko
 */

#ifndef POLY_HPP_
#define POLY_HPP_

#include <gsl/gsl_poly.h>
#include <gsl/gsl_vector.h>

#include <boost/random/random_device.hpp>
#include <boost/random/uniform_int_distribution.hpp>
#include <boost/scoped_ptr.hpp>

#include <log4cxx/logger.h>
#include "log4cxx/basicconfigurator.h"

using boost::scoped_ptr;

class Poly {

public:

  Poly( int64_t degree, double y0);

  void init(double y0);

  void set_y0(double y0);

  double eval(double value);


  /*
   * We need to use a strong, non-pseudo random generator.
   */
  boost::random::random_device rng_;
  static log4cxx::LoggerPtr logger_;

private:
  /*
   * A random polynomial used in the secret sharing scheme.
   */
  scoped_ptr<gsl_vector> poly_;
  /*
   * A degree of the above polynomial.
   */
   int64_t degree_;
  /*
   * The point where the polynomial intersect the x-axis.
   */
  double y0_;

};

#endif /* POLY_HPP_ */
