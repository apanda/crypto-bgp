
#include <secret_sharing/poly.hpp>

#include <common.hpp>

log4cxx::LoggerPtr Poly::logger_(log4cxx::Logger::getLogger("Peer"));


Poly::Poly(int degree, double y0) :
    degree_(degree),
    y0_(y0),
    poly_(gsl_vector_alloc(degree)) {

  init(y0);
  set_y0(y0);
}



void Poly::set_y0(double y0) {
  gsl_vector_set(poly_.get(), 0, y0);
}



void Poly::init(double y0) {

  set_y0(y0);
  boost::random::uniform_int_distribution<> dist(0, PRIME - 1);
  for (auto i = 1; i < degree_; i++) {

    auto coefficient = dist(rng_);
    gsl_vector_set(poly_.get(), i, coefficient);
  }

}



double Poly::eval(double value) {
  int inter = gsl_poly_eval(poly_->data, degree_, value);
  return mod(inter, PRIME);
  //return inter;
}
