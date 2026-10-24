// The interface's file chooser. Own design; see PROVENANCE.md.

#include "file_browser.hpp"

#include "ui/text.hpp"

#include <misc/cpp/imgui_stdlib.h>

#include <cstdio>
#include <system_error>

namespace tisma::ui {
namespace {

std::string size_text(std::uintmax_t bytes) {
    char text[32];
    if (bytes >= 1024 * 1024)
        std::snprintf(text, sizeof text, "%.1f MB", double(bytes) / (1024.0 * 1024.0));
    else
        std::snprintf(text, sizeof text, "%.0f KB", double(bytes) / 1024.0 + 0.5);
    return text;
}

} // namespace

void FileBrowser::open(Mode mode, std::string title, std::vector<std::string> extensions,
                       const std::filesystem::path& folder, std::string name) {
    mode_ = mode;
    title_ = std::move(title);
    extensions_ = std::move(extensions);
    name_ = std::move(name);
    show_all_ = false;
    confirm_replace_ = false;
    std::error_code ec;
    std::filesystem::path start = folder;
    if (start.empty() || !std::filesystem::is_directory(start, ec)) {
        const std::vector<std::filesystem::path> roots = folder_roots();
        start = roots.empty() ? std::filesystem::current_path(ec) : roots.front();
    }
    go(start);
    open_ = true;
    pending_open_ = true;
}

void FileBrowser::go(const std::filesystem::path& folder) {
    std::error_code ec;
    std::filesystem::path f = std::filesystem::absolute(folder, ec);
    folder_ = (ec ? folder : f).lexically_normal();
    readable_ = list_folder(folder_, show_all_ ? std::vector<std::string>{} : extensions_, entries_);
    folder_text_ = utf8(folder_);
    selected_ = -1;
}

std::filesystem::path FileBrowser::target() const {
    std::filesystem::path name = from_utf8(name_);
    if (mode_ == Mode::Save && !extensions_.empty() && lower_extension(name).empty())
        name += from_utf8(extensions_.front());
    return name.is_absolute() ? name : folder_ / name;
}

bool FileBrowser::draw(std::filesystem::path& chosen, ImTextureRef folder_icon, ImVec4 icon_tint) {
    if (!open_)
        return false;
    const std::string popup = title_ + "###file_browser";
    if (pending_open_) {
        ImGui::OpenPopup(popup.c_str());
        pending_open_ = false;
    }
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x * 0.6f, viewport->Size.y * 0.7f), ImGuiCond_Appearing);
    bool picked = false;
    if (!ImGui::BeginPopupModal(popup.c_str(), &open_)) {
        open_ = ImGui::IsPopupOpen(popup.c_str());
        return false;
    }
    const float line = ImGui::GetFrameHeight();

    // The folder: up, the places to start from, and the path, which can be typed.
    if (ImGui::Button(tr(T::Up)) && folder_.has_parent_path() && folder_.parent_path() != folder_)
        go(folder_.parent_path());
    ImGui::SameLine();
    ImGui::SetNextItemWidth(line * 5.0f);
    if (ImGui::BeginCombo("##roots", tr(T::Folder))) {
        for (const std::filesystem::path& root : folder_roots())
            if (ImGui::Selectable(utf8(root).c_str()))
                go(root);
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::InputText("##folder", &folder_text_, ImGuiInputTextFlags_EnterReturnsTrue)) {
        std::error_code ec;
        if (std::filesystem::is_directory(from_utf8(folder_text_), ec))
            go(from_utf8(folder_text_));
        else
            folder_text_ = utf8(folder_);
    }

    const float footer = line * (mode_ == Mode::Save ? 2.6f : 1.5f) + ImGui::GetStyle().ItemSpacing.y * 2.0f;
    if (ImGui::BeginChild("##entries", ImVec2(0, -footer), ImGuiChildFlags_Borders)) {
        if (!readable_)
            ImGui::TextDisabled("%s", tr(T::CannotOpen));
        std::filesystem::path enter;
        for (int i = 0; i < int(entries_.size()); ++i) {
            const FolderEntry& e = entries_[std::size_t(i)];
            ImGui::PushID(i);
            const ImVec2 start = ImGui::GetCursorPos();
            const float right = start.x + ImGui::GetContentRegionAvail().x;
            if (ImGui::Selectable("##entry", selected_ == i,
                                  ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_SpanAllColumns,
                                  ImVec2(0, line))) {
                selected_ = i;
                if (!e.folder)
                    name_ = e.name;
                if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    if (e.folder)
                        enter = e.path;
                    else
                        picked = true;
                }
            }
            ImGui::SetCursorPos(ImVec2(start.x, start.y + (line - ImGui::GetTextLineHeight()) * 0.5f));
            const float icon = ImGui::GetTextLineHeight();
            if (e.folder)
                ImGui::ImageWithBg(folder_icon, ImVec2(icon, icon), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0),
                                   icon_tint);
            else
                ImGui::Dummy(ImVec2(icon, icon));
            ImGui::SameLine();
            ImGui::TextUnformatted(e.name.c_str());
            if (!e.folder) {
                const std::string size = size_text(e.size);
                ImGui::SameLine(right - ImGui::CalcTextSize(size.c_str()).x);
                ImGui::TextDisabled("%s", size.c_str());
            }
            ImGui::SetCursorPos(ImVec2(start.x, start.y + line + ImGui::GetStyle().ItemSpacing.y));
            ImGui::PopID();
        }
        if (!enter.empty())
            go(enter);
    }
    ImGui::EndChild();

    if (mode_ == Mode::Save) {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(tr(T::FileName));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputText("##name", &name_, ImGuiInputTextFlags_EnterReturnsTrue))
            picked = true;
    }
    const float right = ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x;
    if (ImGui::Checkbox(tr(T::ShowAllFiles), &show_all_))
        go(folder_);
    const float buttons = ImGui::CalcTextSize(tr(T::Cancel)).x + ImGui::CalcTextSize(tr(T::SaveButton)).x +
                          ImGui::GetStyle().FramePadding.x * 4.0f + ImGui::GetStyle().ItemSpacing.x + line * 2.0f;
    ImGui::SameLine(right - buttons);
    if (ImGui::Button(tr(T::Cancel)))
        open_ = false;
    ImGui::SameLine();
    const bool can_pick = !name_.empty();
    ImGui::BeginDisabled(!can_pick);
    if (ImGui::Button(mode_ == Mode::Save ? tr(T::SaveButton) : tr(T::OpenButton)))
        picked = true;
    ImGui::EndDisabled();

    if (picked && can_pick) {
        std::error_code ec;
        const std::filesystem::path path = target();
        if (std::filesystem::is_directory(path, ec)) {
            go(path);
            name_.clear();
        } else if (mode_ == Mode::Save && std::filesystem::exists(path, ec) && !confirm_replace_) {
            confirm_replace_ = true;
        } else if (mode_ == Mode::Open && !std::filesystem::exists(path, ec)) {
            // A name typed for a file that is not there: nothing to open.
        } else {
            chosen = path;
            open_ = false;
            confirm_replace_ = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return true;
        }
    }
    if (confirm_replace_) {
        ImGui::Separator();
        ImGui::TextUnformatted(tr(T::Replace));
        ImGui::SameLine();
        if (ImGui::Button(tr(T::Yes))) {
            chosen = target();
            open_ = false;
            confirm_replace_ = false;
            ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
            return true;
        }
        ImGui::SameLine();
        if (ImGui::Button(tr(T::No)))
            confirm_replace_ = false;
    }
    if (!open_)
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
    return false;
}

} // namespace tisma::ui
