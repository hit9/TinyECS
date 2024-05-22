#include <catch2/catch_test_macros.hpp>

#include "shares.h"
#include "tinyecs.h"

using namespace tinyecs;
using namespace tinyecs_tests;

TEST_CASE("component id", "[unit component_test.cc]") {
  auto aid = __internal::IComponent<A>::GetId();
  auto bid = __internal::IComponent<B>::GetId();
  auto cid = __internal::IComponent<C>::GetId();
  REQUIRE(aid == 0);
  REQUIRE(bid == 1);
  REQUIRE(cid == 2);
  REQUIRE(aid == __internal::IComponent<A>::GetId());
}

TEST_CASE("component sequence", "[unit component_test.cc]") {
  const auto &s1 = __internal::ComponentSequence<A, B>::GetSignature();
  const auto &s2 = __internal::ComponentSequence<B, C>::GetSignature();
  const auto &s3 = __internal::ComponentSequence<A, B>::GetSignature();
  const auto &s4 = __internal::ComponentSequence<A, B, C>::GetSignature();
  const auto &s5 = __internal::ComponentSequence<C, B, A>::GetSignature();
  REQUIRE(&s1 == &s3);
  REQUIRE(s2 != s1);
  // content equals, but pointer differs
  REQUIRE(s4 == s5);
  REQUIRE(&s4 != &s5);
  // What's more, we know exactly whare they are.
  REQUIRE(s4[0]);
  REQUIRE(s4[1]);
  REQUIRE(s4[2]);
  REQUIRE(!s4[3]);
}
