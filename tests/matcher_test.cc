#include <catch2/catch_test_macros.hpp>

#include "shares.h"
#include "tinyecs.h"

using namespace tinyecs;
using namespace tinyecs_tests;
template <typename... Components> using _CS = __internal::ComponentSequence<Components...>;
using tinyecs::__internal::AIds;
using tinyecs::__internal::MatchRelation;

TEST_CASE("matcher", "[simple]") {
  __internal::Matcher m;
  m.PutArchetypeId(_CS<A, B, C>::GetSignature(), 1);

  REQUIRE(m.Match(MatchRelation::ALL, _CS<A>::GetSignature()).size());
  REQUIRE(m.Match(MatchRelation::ALL, _CS<A, B, C>::GetSignature()).size());
  REQUIRE(m.Match(MatchRelation::ALL, _CS<A, B>::GetSignature()).size());
  REQUIRE(m.Match(MatchRelation::ALL, _CS<A, C>::GetSignature()).size());
  REQUIRE(m.Match(MatchRelation::ALL, _CS<C, B, A>::GetSignature()).size());

  REQUIRE(m.Match(MatchRelation::ANY, _CS<A, B>::GetSignature()).size());
  REQUIRE(m.Match(MatchRelation::ANY, _CS<A, D>::GetSignature()).size());

  REQUIRE(m.Match(MatchRelation::NONE, _CS<D>::GetSignature()).size());
  REQUIRE(m.Match(MatchRelation::NONE, _CS<E, D>::GetSignature()).size());

  REQUIRE(m.Match(MatchRelation::ALL, _CS<A, D>::GetSignature()).empty());
  REQUIRE(m.Match(MatchRelation::ALL, _CS<D>::GetSignature()).empty());
  REQUIRE(m.Match(MatchRelation::ALL, _CS<B, D>::GetSignature()).empty());
  REQUIRE(m.Match(MatchRelation::ALL, _CS<A, B, C, D>::GetSignature()).empty());

  REQUIRE(m.Match(MatchRelation::ANY, _CS<D>::GetSignature()).empty());
  REQUIRE(m.Match(MatchRelation::ANY, _CS<D, E>::GetSignature()).empty());
}

TEST_CASE("matcher", "[multiple]") {
  __internal::Matcher m;

  m.PutArchetypeId(_CS<C, A, B>::GetSignature(), 1);
  m.PutArchetypeId(_CS<A, B>::GetSignature(), 2);
  m.PutArchetypeId(_CS<A, D, E>::GetSignature(), 3);
  m.PutArchetypeId(_CS<E, D, B>::GetSignature(), 4);

  //~~~~~~~~~~ MatchAll~~~~~~~~~~
  REQUIRE(m.Match(MatchRelation::ALL, _CS<A>::GetSignature()) == AIds{1, 2, 3});
  REQUIRE(m.Match(MatchRelation::ALL, _CS<B, A>::GetSignature()) == AIds{1, 2});
  REQUIRE(m.Match(MatchRelation::ALL, _CS<E>::GetSignature()) == AIds{3, 4});
  REQUIRE(m.Match(MatchRelation::ALL, _CS<D, E>::GetSignature()) == AIds{3, 4});
  REQUIRE(m.Match(MatchRelation::ALL, _CS<A, E>::GetSignature()) == AIds{3});
  REQUIRE(m.Match(MatchRelation::ALL, _CS<A, B, C, D>::GetSignature()) == AIds{});
  //~~~~~~~~~~ MatchAny~~~~~~~~~~
  REQUIRE(m.Match(MatchRelation::ANY, _CS<A, B>::GetSignature()) == AIds{1, 2, 3, 4});
  REQUIRE(m.Match(MatchRelation::ANY, _CS<E>::GetSignature()) == AIds{3, 4});
  REQUIRE(m.Match(MatchRelation::ANY, _CS<E, C>::GetSignature()) == AIds{1, 3, 4});
  REQUIRE(m.Match(MatchRelation::ANY, _CS<F>::GetSignature()) == AIds{});
  // Test match any exist archetype ids
  REQUIRE(m.Match(MatchRelation::ANY, _CS<>::GetSignature()) == AIds{1, 2, 3, 4});

  //~~~~~~~~~~ MatchNone~~~~~~~~~~
  REQUIRE(m.Match(MatchRelation::NONE, _CS<F>::GetSignature()) == AIds{1, 2, 3, 4});
  REQUIRE(m.Match(MatchRelation::NONE, _CS<B, A>::GetSignature()).empty());
  REQUIRE(m.Match(MatchRelation::NONE, _CS<C, E>::GetSignature()) == AIds{2});
}

TEST_CASE("archetype m", "[bugfix 1]") {
  __internal::Matcher m;
  m.PutArchetypeId(_CS<D, E, F>::GetSignature(), 1);
  REQUIRE(m.Match(MatchRelation::ALL, _CS<D, F>::GetSignature()) == AIds{1});
}
