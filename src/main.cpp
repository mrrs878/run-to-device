/*
 * @Author: mrrs878@foxmail.com
 * @Date: 2025-09-25 06:33:21
 * @LastEditors: mrrs878@foxmail.com
 * @LastEditTime: 2025-09-25 06:33:24
 */

#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <string>
#include <vector>
#include <deque>
#include <mutex>
#include <memory>

using namespace ftxui;

int main()
{
    auto screen = ScreenInteractive::FitComponent();

    // Model
    std::deque<std::string> log_lines;
    std::mutex log_mutex;

    std::vector<std::string> commands = {
        "connect", "disconnect", "devices", "logcat", "screenrecord", "screenshot", "help", "version"};

    std::string input_buffer;
    bool show_completions = false;
    int completion_index = 0;

    // Input component
    InputOption style = InputOption::Default();
    style.transform = [](InputState state) {
        state.element |= color(Color::White);

        if (state.is_placeholder) {
            state.element |= dim;
        }

        state.element |= border;

        state.element |= focus;

        return state.element;
    };
    auto input = Input(&input_buffer, "Type a command. Use '/' to trigger completions.", style);

    // Helper to append to log
    auto append_log = [&](const std::string &line)
    {
        std::lock_guard<std::mutex> guard(log_mutex);
        log_lines.push_back(line);
        if (log_lines.size() > 1000)
            log_lines.pop_front();
    };

    // Initial sample
    append_log("quick-adb v0.0.1");
    append_log("Welcome to the prototype. Type a command below.");

    // Log renderer: renders log_lines into a paragraph
    auto log_renderer = Renderer([&]
                                 {
    Elements lines;
    std::lock_guard<std::mutex> guard(log_mutex);
    for (auto &l : log_lines) {
      lines.push_back(text(l));
    }
    auto para = vbox(std::move(lines));
    return window(text(" Command Log ") | bold, para) | flex; });

    // Completions menu renderer
    auto completions_renderer = Renderer([&]
                                         {
    if(!show_completions) return text("");
    Elements items;
    for(int i=0;i<(int)commands.size();++i){
      auto label = commands[i];
      if(i==completion_index) items.push_back(text(label) | inverted);
      else items.push_back(text(label));
    }
    return vbox(std::move(items)) | border; });

    // Wrap input so we can intercept keys
    auto input_with_handler = CatchEvent(input, [&](Event event)
                                         {
    // If completions are visible, handle navigation and selection first
    if(show_completions) {
      if(event == Event::ArrowDown) { completion_index = (completion_index + 1) % commands.size(); return true; }
      if(event == Event::ArrowUp) { completion_index = (completion_index - 1 + commands.size()) % commands.size(); return true; }
      if(event == Event::Return) {
        // Fill input with completion. Preserve leading '/'
        if(!input_buffer.empty() && input_buffer.front() == '/')
          input_buffer = std::string("/") + commands[completion_index] + " ";
        else
          input_buffer = std::string("/") + commands[completion_index] + " ";
        show_completions = false;
        return true;
      }
    }

    if(event == Event::Escape) {
      // Clear input / hide completions
      input_buffer.clear();
      show_completions = false;
      return true;
    }
    if(event == Event::Character('/') ) {
      show_completions = true;
      completion_index = 0;
      return false; // let the '/' go into input
    }

    if(event == Event::Return) {
      // Echo command to log and clear input
      append_log(std::string("> ") + input_buffer);
      input_buffer.clear();
      show_completions = false;
      return true;
    }
    return false; });

    // Header renderer
    auto header_renderer = Renderer([&]
                                    { return (hbox({text("QUICK ADB") | bold | center, text("quick-adb v0.0.1") | dim}) | border); });

    // Compose header + log as one renderer
    auto header_and_log = Renderer([&]
                                   { return vbox({header_renderer->Render(), separator(), log_renderer->Render()}); });

    // Decorate the input component with a simple border for better visual.
    // Use vbox + border so background is not forced — it will use terminal background.
    // Decorate the input component with a simple border for better visual.
    // Use vbox + border so background is not forced — it will use terminal background.
    auto input_box = Renderer(input_with_handler, [&]
                              { return hbox( {input_with_handler->Render() }); });

    // Build a container so children can receive focus. Order: header/log, input, completions
    int selector = 1; // select the input component by default (index 1)
    Components children;
    children.push_back(header_and_log);
    children.push_back(input_box);
    children.push_back(completions_renderer);
    auto container = Container::Vertical(children, &selector);

    // Wrap container to catch a global help key
    auto main_component = CatchEvent(container, [&](Event event)
                                     {
    if(event == Event::Character('?')){
      append_log("Help: Type commands. '/' triggers completions. Enter to run.");
      return true;
    }
    return false; });

    screen.Loop(main_component);

    return 0;
}
