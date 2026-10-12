#pragma once

// The whole app: screens, navigation, playback. See docs/DESIGN.md.
class App {
public:
    App();
    ~App();
    void run();

private:
    struct State;
    State *s_;
};
