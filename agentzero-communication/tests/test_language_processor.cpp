#include "test_runner.h"

#include <opencog/agentzero/communication/LanguageProcessor.h>
#include <opencog/atomspace/AtomSpace.h>
#include <opencog/atoms/atom_types/types.h>

using namespace opencog;
using namespace opencog::agentzero::communication;

static AtomSpacePtr make_as() { return createAtomSpace(); }

TEST(LanguageProcessor_NullAtomSpaceThrows)
{
    bool threw = false;
    try {
        LanguageProcessor lp(nullptr);
    } catch (const std::exception&) {
        threw = true;
    }
    ASSERT_TRUE(threw);
}

TEST(LanguageProcessor_ParseGreeting)
{
    auto as = make_as();
    LanguageProcessor lp(as);

    auto result = lp.parseText("Hello there!");
    ASSERT_TRUE(result.success);
    ASSERT_STREQ(result.intent.c_str(), "greeting");
    ASSERT_FALSE(result.tokens.empty());
    ASSERT_FALSE(result.parsed_atoms.empty());
    ASSERT_GT(result.confidence, 0.4);
    ASSERT_EQ(lp.getParseCount(), static_cast<size_t>(1));
}

TEST(LanguageProcessor_DetectIntents)
{
    auto as = make_as();
    LanguageProcessor lp(as);

    ASSERT_STREQ(lp.detectIntent("Hi agent").c_str(), "greeting");
    ASSERT_STREQ(lp.detectIntent("Goodbye for now").c_str(), "farewell");
    ASSERT_STREQ(lp.detectIntent("What is your name?").c_str(), "question");
    ASSERT_STREQ(lp.detectIntent("Please help me plan").c_str(), "request");
    ASSERT_STREQ(lp.detectIntent("The sky is blue").c_str(), "statement");
    ASSERT_STREQ(lp.detectIntent("").c_str(), "unknown");
}

TEST(LanguageProcessor_EntitiesAndTokens)
{
    auto as = make_as();
    LanguageProcessor lp(as);

    auto tokens = lp.tokenize("Alice met Bob in Paris.");
    ASSERT_GE(tokens.size(), static_cast<size_t>(4));

    auto entities = lp.extractEntities("Alice met Bob in \"New York\".");
    ASSERT_GE(entities.size(), static_cast<size_t>(2));
}

TEST(LanguageProcessor_GenerateResponses)
{
    auto as = make_as();
    LanguageProcessor lp(as);

    auto greet = lp.generateResponse("Hello!");
    ASSERT_FALSE(greet.empty());
    ASSERT_NE(greet.find("Hello"), std::string::npos);

    auto bye = lp.generateResponse("Goodbye");
    ASSERT_NE(bye.find("Goodbye"), std::string::npos);

    auto q = lp.generateResponse("Who are you?");
    ASSERT_FALSE(q.empty());

    auto themed = lp.generateResponse("Tell me more", "topic:planning");
    ASSERT_NE(themed.find("planning"), std::string::npos);

    ASSERT_EQ(lp.getResponseCount(), static_cast<size_t>(4));
}

TEST(LanguageProcessor_TextAtomRoundTrip)
{
    auto as = make_as();
    LanguageProcessor lp(as);

    Handle h = lp.textToAtoms("Cats sleep often");
    ASSERT_TRUE(h != Handle::UNDEFINED);
    ASSERT_GT(as->get_size(), static_cast<size_t>(0));

    std::string back = lp.atomsToText({h});
    ASSERT_NE(back.find("Cats sleep often"), std::string::npos);
}

TEST(LanguageProcessor_EmptyParseFails)
{
    auto as = make_as();
    LanguageProcessor lp(as);
    auto r = lp.parseText("");
    ASSERT_FALSE(r.success);
    ASSERT_TRUE(lp.generateResponse("").empty());
}
