
#include <secret_sharing/poly.hpp>

#include <common.hpp>

log4cxx::LoggerPtr Poly::logger_(log4cxx::Logger::getLogger("Peer"));


Poly::Poly(int degree, double y0) :
    poly_(gsl_vector_alloc(degree)),
    degree_(degree),
    y0_(y0) {

  init(y0);
  set_y0(y0);
}


Poly::~Poly() {
  gsl_vector_free(poly_);
}


void Poly::set_y0(double y0) {
  gsl_vector_set(poly_, 0, y0);
}



void Poly::init(double y0) {

  set_y0(y0);
  boost::random::uniform_int_distribution<> dist(-8, 8);

  for (auto i = 1; i < degree_; i++) {

    int coefficient = dist(rng_);
    gsl_vector_set(poly_, i, coefficient);
  }

}



double Poly::eval(double value) {
  int64_t inter = gsl_poly_eval(poly_->data, degree_, value);
  //return mod(inter, PRIME);
  return inter;
}
