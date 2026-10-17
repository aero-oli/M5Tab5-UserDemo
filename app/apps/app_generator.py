#!/usr/bin/env python3
import argparse
import re
from datetime import datetime
from pathlib import Path


SCRIPT_DIRECTORY = Path(__file__).resolve().parent


def to_snake_case(value: str) -> str:
    words = re.sub(r"[^A-Za-z0-9]+", " ", value).strip().split()
    return "_".join(word.lower() for word in words)


def to_pascal_case(value: str) -> str:
    return "".join(word.capitalize() for word in to_snake_case(value).split("_"))


def check_input_valid(value: str) -> bool:
    snake = to_snake_case(value)
    return bool(snake) and snake != "template"


def header_template(namespace: str, factory_name: str) -> str:
    return f"""/*
 * SPDX-FileCopyrightText: {datetime.now().year} M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#pragma once

#include <memory>
#include <mooncake.h>

namespace {namespace} {{

std::unique_ptr<mooncake::AppAbility> {factory_name}();

}}  // namespace {namespace}
"""


def source_template(class_name: str, namespace: str, header_name: str, title: str, factory_name: str) -> str:
    return f"""/*
 * SPDX-FileCopyrightText: {datetime.now().year} M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "{header_name}"

#include <memory>
#include <hal/hal.h>
#include <lvgl.h>
#include <mooncake.h>
#include <smooth_lvgl.h>
#include <apps/ui/app_shell.h>
#include <apps/ui/ui_components.h>
#include <apps/ui/ui_theme.h>

using namespace {namespace};

namespace {{

class {class_name} : public mooncake::AppAbility {{
public:
    {class_name}()
    {{
        setAppInfo().name = "{title}";
    }}

    void onOpen() override
    {{
        _shell = std::make_unique<app_ui::AppShell>("{title}", [&]() {{ close(); }});
        _shell->init();
        build();
    }}

    void onClose() override
    {{
        _shell.reset();
    }}

private:
    std::unique_ptr<app_ui::AppShell> _shell;

    lv_obj_t* content() const
    {{
        return _shell->content();
    }}

    void build()
    {{
        LvglLockGuard lock;
        auto* title = app_ui::makeLabel(content(), "{title}", 0, 0, &lv_font_montserrat_24,
                                        app_ui::theme::color(app_ui::theme::text));
        lv_obj_set_width(title, 640);
        lv_label_set_long_mode(title, LV_LABEL_LONG_DOT);

        auto* detail = app_ui::makeLabel(content(), "Add controls, controller state, and layout helpers here.", 0, 56,
                                         &lv_font_montserrat_18, app_ui::theme::color(app_ui::theme::muted));
        lv_obj_set_width(detail, 900);
        lv_label_set_long_mode(detail, LV_LABEL_LONG_WRAP);
    }}
}};

}}  // namespace

std::unique_ptr<mooncake::AppAbility> {namespace}::{factory_name}()
{{
    return std::make_unique<{class_name}>();
}}
"""


def catalog_snippet(app_key: str, namespace: str, header_path: str, factory_name: str, title: str, icon: str) -> str:
    return f"""Add the app key in app/apps/app_registry.h:
    {app_key},

Add the include in app/apps/app_catalog.cpp:
    #include "{header_path}"

Add the catalog entry in app_catalog::registerDefaultApps():
    apps.add({{AppKey::{app_key},
              AppCategory::Settings,
              "{title}",
              "Describe this app",
              "",
              "{icon}",
              100,
              0,
              false,
              false,
              [] {{ return {namespace}::{factory_name}(); }},
              StatusProvider{{}}}});
"""


def create_app_files(raw_name: str) -> None:
    if not check_input_valid(raw_name):
        raise ValueError("Use a non-empty app name containing letters, numbers, spaces, underscores, or dashes.")

    feature = to_snake_case(raw_name)
    pascal = to_pascal_case(raw_name)
    class_name = f"App{pascal}"
    namespace = f"app_{feature}"
    folder_name = f"app_{feature}"
    folder = SCRIPT_DIRECTORY / folder_name
    header_name = f"{folder_name}.h"
    source_name = f"{folder_name}.cpp"
    header_path = folder / header_name
    source_path = folder / source_name
    factory_name = f"make{pascal}App"
    title = raw_name.strip().replace("_", " ").replace("-", " ").title()
    icon = "".join(part[0].upper() for part in title.split()[:2]) or "AP"

    folder.mkdir(exist_ok=False)
    header_path.write_text(header_template(namespace, factory_name), encoding="utf-8")
    source_path.write_text(source_template(class_name, namespace, header_name, title, factory_name), encoding="utf-8")

    print(f"Created {header_path}")
    print(f"Created {source_path}")
    print()
    print(catalog_snippet(pascal, namespace, f"{folder_name}/{header_name}", factory_name, title, icon[:2]))
    print("Re-run CMake after adding the catalog entry so new source files are picked up.")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Create a modular Mooncake app shell.")
    parser.add_argument("name", nargs="?", help="New app name, for example 'Fan Control'")
    return parser.parse_args()


if __name__ == "__main__":
    args = parse_args()
    name = args.name or input("input new app's name: ")
    create_app_files(name)
