#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "session.h"
#include "session_manager.h"

namespace {

class SessionTest : public ::testing::Test {
};

TEST_F(SessionTest, DefaultConstructor) {
    oj::Session s;
    EXPECT_EQ(s.getToken(), "");
    EXPECT_EQ(s.getUserId(), 0);
    EXPECT_EQ(s.getUsername(), "");
    EXPECT_EQ(s.getRole(), "");
    EXPECT_EQ(s.getCreatedAt(), 0);
    EXPECT_EQ(s.getExpiresAt(), 0);
}

TEST_F(SessionTest, ParameterizedConstructor) {
    oj::Session s("token123", 42, "testuser", "admin");

    EXPECT_EQ(s.getToken(), "token123");
    EXPECT_EQ(s.getUserId(), 42);
    EXPECT_EQ(s.getUsername(), "testuser");
    EXPECT_EQ(s.getRole(), "admin");
    EXPECT_GT(s.getCreatedAt(), 0);
    EXPECT_GT(s.getExpiresAt(), s.getCreatedAt());
}

TEST_F(SessionTest, GettersAndSetters) {
    oj::Session s;
    s.setToken("newtoken");
    s.setUserId(100);
    s.setUsername("newuser");
    s.setRole("user");
    s.setCreatedAt(1000);
    s.setExpiresAt(2000);

    EXPECT_EQ(s.getToken(), "newtoken");
    EXPECT_EQ(s.getUserId(), 100);
    EXPECT_EQ(s.getUsername(), "newuser");
    EXPECT_EQ(s.getRole(), "user");
    EXPECT_EQ(s.getCreatedAt(), 1000);
    EXPECT_EQ(s.getExpiresAt(), 2000);
}

TEST_F(SessionTest, IsExpiredFalseWhenNotExpired) {
    oj::Session s("token", 1, "user", "user");
    EXPECT_FALSE(s.isExpired());
}

TEST_F(SessionTest, IsExpiredTrueWhenExpired) {
    oj::Session s("token", 1, "user", "user");
    s.setExpiresAt(time(nullptr) - 1);
    EXPECT_TRUE(s.isExpired());
}

class SessionManagerTest : public ::testing::Test {
protected:
    void TearDown() override {
        auto& manager = oj::SessionManager::getInstance();
        std::string token = manager.createSession(999, "cleanup", "user");
        manager.destroySession(token);
    }
};

TEST_F(SessionManagerTest, CreateSessionReturnsToken) {
    auto& manager = oj::SessionManager::getInstance();
    std::string token = manager.createSession(1, "testuser", "user");

    EXPECT_FALSE(token.empty());
    EXPECT_EQ(token.size(), 32U);

    manager.destroySession(token);
}

TEST_F(SessionManagerTest, CreateSessionCreatesUniqueTokens) {
    auto& manager = oj::SessionManager::getInstance();

    std::string token1 = manager.createSession(1, "user1", "user");
    std::string token2 = manager.createSession(2, "user2", "user");

    EXPECT_NE(token1, token2);

    manager.destroySession(token1);
    manager.destroySession(token2);
}

TEST_F(SessionManagerTest, GetSessionReturnsValidSession) {
    auto& manager = oj::SessionManager::getInstance();
    std::string token = manager.createSession(42, "testuser", "admin");

    oj::Session* session = manager.getSession(token);

    ASSERT_NE(session, nullptr);
    EXPECT_EQ(session->getUserId(), 42);
    EXPECT_EQ(session->getUsername(), "testuser");
    EXPECT_EQ(session->getRole(), "admin");

    manager.destroySession(token);
}

TEST_F(SessionManagerTest, GetSessionReturnsNullptrForInvalidToken) {
    auto& manager = oj::SessionManager::getInstance();
    oj::Session* session = manager.getSession("nonexistent_token");

    EXPECT_EQ(session, nullptr);
}

TEST_F(SessionManagerTest, DestroySessionRemovesSession) {
    auto& manager = oj::SessionManager::getInstance();
    std::string token = manager.createSession(1, "user", "user");

    bool destroyed = manager.destroySession(token);
    EXPECT_TRUE(destroyed);

    oj::Session* session = manager.getSession(token);
    EXPECT_EQ(session, nullptr);
}

TEST_F(SessionManagerTest, DestroySessionReturnsFalseForInvalidToken) {
    auto& manager = oj::SessionManager::getInstance();
    bool destroyed = manager.destroySession("invalid_token");

    EXPECT_FALSE(destroyed);
}

TEST_F(SessionManagerTest, SessionDataPersistsUntilDestroyed) {
    auto& manager = oj::SessionManager::getInstance();
    std::string token = manager.createSession(123, "persistuser", "admin");

    for (int i = 0; i < 5; ++i) {
        oj::Session* session = manager.getSession(token);
        ASSERT_NE(session, nullptr);
        EXPECT_EQ(session->getUserId(), 123);
        EXPECT_EQ(session->getUsername(), "persistuser");
        EXPECT_EQ(session->getRole(), "admin");
    }

    manager.destroySession(token);
}

TEST_F(SessionManagerTest, MultipleSessionsCanExistSimultaneously) {
    auto& manager = oj::SessionManager::getInstance();

    std::string token1 = manager.createSession(1, "user1", "user");
    std::string token2 = manager.createSession(2, "user2", "user");
    std::string token3 = manager.createSession(3, "user3", "admin");

    {
        oj::Session* s1 = manager.getSession(token1);
        oj::Session* s2 = manager.getSession(token2);
        oj::Session* s3 = manager.getSession(token3);

        ASSERT_NE(s1, nullptr);
        ASSERT_NE(s2, nullptr);
        ASSERT_NE(s3, nullptr);

        EXPECT_EQ(s1->getUserId(), 1);
        EXPECT_EQ(s2->getUserId(), 2);
        EXPECT_EQ(s3->getUserId(), 3);
        EXPECT_EQ(s3->getRole(), "admin");
    }

    manager.destroySession(token1);
    manager.destroySession(token2);
    manager.destroySession(token3);
}

TEST_F(SessionManagerTest, CleanExpiredSessionsRemovesOnlyExpired) {
    auto& manager = oj::SessionManager::getInstance();

    std::string validToken = manager.createSession(1, "validuser", "user");
    std::string expiredToken = manager.createSession(2, "expireduser", "user");

    auto* expiredSession = manager.getSession(expiredToken);
    ASSERT_NE(expiredSession, nullptr);
    expiredSession->setExpiresAt(time(nullptr) - 10);

    manager.cleanExpiredSessions();

    oj::Session* validSession = manager.getSession(validToken);
    EXPECT_NE(validSession, nullptr);

    oj::Session* expiredSessionAfter = manager.getSession(expiredToken);
    EXPECT_EQ(expiredSessionAfter, nullptr);

    manager.destroySession(validToken);
}

TEST_F(SessionManagerTest, SessionSurvivesOtherSessionDestruction) {
    auto& manager = oj::SessionManager::getInstance();

    std::string token1 = manager.createSession(1, "user1", "user");
    std::string token2 = manager.createSession(2, "user2", "user");

    manager.destroySession(token1);

    oj::Session* session2 = manager.getSession(token2);
    ASSERT_NE(session2, nullptr);
    EXPECT_EQ(session2->getUserId(), 2);

    manager.destroySession(token2);
}

TEST_F(SessionManagerTest, ConcurrentSessionCreation) {
    auto& manager = oj::SessionManager::getInstance();
    std::vector<std::string> tokens;
    std::mutex mutex;

    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&manager, &tokens, &mutex, i]() {
            std::string token = manager.createSession(i, "user" + std::to_string(i), "user");
            std::lock_guard<std::mutex> lock(mutex);
            tokens.push_back(token);
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(tokens.size(), 10U);

    for (const auto& token : tokens) {
        auto* session = manager.getSession(token);
        ASSERT_NE(session, nullptr);
        manager.destroySession(token);
    }
}

TEST_F(SessionManagerTest, ConcurrentSessionAccess) {
    auto& manager = oj::SessionManager::getInstance();
    std::string token = manager.createSession(1, "user", "user");

    std::atomic<int> successCount{0};
    std::vector<std::thread> threads;

    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&manager, &token, &successCount]() {
            for (int j = 0; j < 100; ++j) {
                auto* session = manager.getSession(token);
                if (session != nullptr && session->getUserId() == 1) {
                    successCount++;
                }
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(successCount, 1000);

    manager.destroySession(token);
}

// ===========================================================================
// Background cleanup thread (B1).
// The thread sweeps expired sessions every N seconds so the in-memory map
// doesn't grow unbounded. We can't easily wait 60s in a test, so these tests
// focus on lifecycle correctness: idempotent start/stop, no crash on
// repeated calls, doesn't block shutdown.
// ===========================================================================

TEST_F(SessionManagerTest, CleanupThreadIdempotentStart) {
    auto& manager = oj::SessionManager::getInstance();
    // Stop whatever may have been started by another test, so we start clean.
    manager.stopCleanupThread();
    manager.startCleanupThread(60);
    manager.startCleanupThread(60); // second call is a no-op
    manager.startCleanupThread(60); // and again
    manager.stopCleanupThread();
}

TEST_F(SessionManagerTest, CleanupThreadStopIsIdempotent) {
    auto& manager = oj::SessionManager::getInstance();
    manager.stopCleanupThread(); // never started → no-op
    manager.stopCleanupThread(); // still no-op
    manager.startCleanupThread(60);
    manager.stopCleanupThread();
    manager.stopCleanupThread(); // second stop → no-op
}

TEST_F(SessionManagerTest, CleanupThreadDoesNotAffectLiveSessions) {
    auto& manager = oj::SessionManager::getInstance();
    manager.stopCleanupThread();

    std::string token = manager.createSession(99, "aliveuser", "user");

    // Even with a short interval, fresh sessions must survive several sweeps.
    manager.startCleanupThread(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    manager.stopCleanupThread();

    auto* session = manager.getSession(token);
    EXPECT_NE(session, nullptr);
    if (session) {
        EXPECT_EQ(session->getUsername(), "aliveuser");
    }
    manager.destroySession(token);
}

TEST_F(SessionManagerTest, CleanupThreadRemovesExpiredSessions) {
    auto& manager = oj::SessionManager::getInstance();
    manager.stopCleanupThread();

    std::string liveToken = manager.createSession(1, "live", "user");
    std::string expiredToken = manager.createSession(2, "dead", "user");

    auto* expiredSession = manager.getSession(expiredToken);
    ASSERT_NE(expiredSession, nullptr);
    expiredSession->setExpiresAt(time(nullptr) - 1);

    // Use a short interval so we can observe the sweep within the test budget.
    manager.startCleanupThread(1);
    std::this_thread::sleep_for(std::chrono::milliseconds(2500));
    manager.stopCleanupThread();

    EXPECT_NE(manager.getSession(liveToken), nullptr);
    EXPECT_EQ(manager.getSession(expiredToken), nullptr);

    manager.destroySession(liveToken);
}

} // namespace
