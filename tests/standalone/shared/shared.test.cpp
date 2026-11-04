
#include <common/shared.h>
#include <common/hit.h>
#define BOOST_TEST_MODULE shared
#include <boost/test/included/unit_test.hpp>

#include <cmath>
#include <memory>
#include <type_traits>

using shared = sand::reco::shared;

namespace {

  struct test_obj : shared {
    test_obj(double x, double y) : m_x(x), m_y(y) {}
    double x() const { return m_x; }
    double y() const { return m_y; }

   protected:
    test_obj(const test_obj&) = default;

   private:
    shared* copy_self() const override { return new test_obj(*this); }

    double m_x;
    double m_y;
  };

} // namespace

static_assert(!std::is_copy_constructible_v<test_obj>, "shared objects must not be copy constructible");
static_assert(!std::is_move_constructible_v<test_obj>, "shared objects must not be move constructible");
static_assert(!std::is_copy_assignable_v<test_obj>, "shared objects must not be copy assignable");
static_assert(!std::is_move_assignable_v<test_obj>, "shared objects must not be move assignable");

BOOST_AUTO_TEST_CASE(create) {
  auto obj = shared::create<test_obj>(1.0, 2.0);
  BOOST_CHECK_EQUAL(obj->x(), 1.0);
  BOOST_CHECK_EQUAL(obj->y(), 2.0);
  BOOST_CHECK_EQUAL(obj->quota(), 1.0);
  BOOST_CHECK(obj->is_unique());
}

BOOST_AUTO_TEST_CASE(split) {
  auto obj = shared::create<test_obj>(1.0, 2.0);
  auto [a, b] = shared::split(std::move(obj), 0.25);
  BOOST_CHECK_EQUAL(a->quota(), 0.25);
  BOOST_CHECK_EQUAL(b->quota(), 0.75);
  BOOST_CHECK(!a->is_unique());
  BOOST_CHECK(!b->is_unique());
  BOOST_CHECK_EQUAL(a->x(), 1.0);
  BOOST_CHECK_EQUAL(b->x(), 1.0);
  BOOST_CHECK_EQUAL(a->y(), 2.0);
  BOOST_CHECK_EQUAL(b->y(), 2.0);
}

BOOST_AUTO_TEST_CASE(nested_split) {
  auto obj = shared::create<test_obj>(1.0, 2.0);
  auto [a, b] = shared::split(std::move(obj), 0.25);
  auto [c, d] = shared::split(std::move(b), 0.5);
  BOOST_CHECK_EQUAL(c->quota(), 0.375);
  BOOST_CHECK_EQUAL(d->quota(), 0.375);
  BOOST_CHECK_EQUAL(a->quota() + c->quota() + d->quota(), 1.0);
}

BOOST_AUTO_TEST_CASE(hit_split) {
  auto h = shared::create<sand::reco::hit>(sand::pos_3d(1.0, 2.0, 3.0), sand::dir_3d(0.0, 0.0, 1.0),
                                           sand::reco::timerange(10.0, 1.0), 5.0);
  BOOST_CHECK(h->is_unique());
  auto [a, b] = shared::split(std::move(h), 0.25);
  BOOST_CHECK_EQUAL(a->quota(), 0.25);
  BOOST_CHECK_EQUAL(b->quota(), 0.75);
  BOOST_CHECK_EQUAL(a->width(), 5.0);
  BOOST_CHECK_EQUAL(b->width(), 5.0);
  BOOST_CHECK(a->centre() == b->centre());
  BOOST_CHECK(a->axis() == b->axis());
  BOOST_CHECK(a->t().best() == b->t().best());
}

BOOST_AUTO_TEST_CASE(default_ctor_is_invalid) {
  sand::reco::hit h;
  BOOST_CHECK(std::isnan(h.quota()));
  BOOST_CHECK(!h.is_unique());
}
