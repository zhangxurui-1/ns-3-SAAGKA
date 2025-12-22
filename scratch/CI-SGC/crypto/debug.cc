#include "../deserialize.h"
#include "../serialize.h"
#include "utils.h"

#include <memory>
#include <pairing_1.h>

void
testG1()
{
    auto pfc = std::make_shared<PFC>(128);
    Big x;
    G1 generator_1;
    pfc->random(x);
    pfc->random(generator_1);
    G1 u = pfc->mult(generator_1, x);

    INFO("test");
    auto bytes = Serialize(u);
    G1 u_;

    Deserialize(bytes.data(), bytes.size(), u_);

    INFO("u=" << ToString(u) << "\n"
              << "u_=" << ToString(u_) << "\nAre they equivalent? " << (u == u_));
}

void
testGT()
{
    auto pfc = std::make_shared<PFC>(128);
    Big x, y;
    G1 X, Y, generator_1;
    pfc->random(generator_1);

    pfc->random(x);
    pfc->random(y);
    X = pfc->mult(generator_1, x);
    Y = pfc->mult(generator_1, y);
    auto K = pfc->pairing(X, Y);

    auto bytes = Serialize(K);
    class GT K_;
    Deserialize(bytes.data(), bytes.size(), K_);
    INFO("K=" << ToString(K) << "\n"
              << "K_=" << ToString(K_) << "\nAre they equivalent? " << (K == K_));
}

void
testBig()
{
    auto pfc = std::make_shared<PFC>(128);
    Big x;
    pfc->random(x);

    auto bytes = Serialize(x);
    Big x_;
    Deserialize(bytes.data(), bytes.size(), x_);
    INFO("x=" << x << "\n"
              << "x_=" << x_ << "\nAre they equivalent? " << (x == x_));
}

int
main()
{
    testG1();
    testGT();
    testBig();
    return 0;
}
