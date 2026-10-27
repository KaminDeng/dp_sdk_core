#!/usr/bin/env python3
"""用CMake File API和真实对象输出核对指定OSAL进程角色的0/1配置。

仅支持当前单配置GCC/Clang编译数据库；不会把独立进程的所有编译行混为一组。
"""
import argparse
from collections import Counter
import json
from pathlib import Path
import shlex
import sys

MACROS = ("DP_OSAL_ENABLE_MEASUREMENT",)


def response_arguments(arguments, directory, stack=()):
    """按编译进程工作目录展开响应文件，拒绝循环和过深嵌套。"""
    output = []
    for argument in arguments:
        if not argument.startswith("@"):
            output.append(argument)
            continue
        path = (directory / argument[1:]).resolve()
        if path in stack or len(stack) >= 16:
            raise ValueError(f"响应文件循环或嵌套过深：{path}")
        output.extend(response_arguments(shlex.split(path.read_text()), directory, (*stack, path)))
    return output


def output_path(arguments, entry, root):
    """命令-o为实际输出；兼容CMake相对build根或工作目录的output元数据。"""
    directory = Path(entry["directory"])
    result = None
    for index, argument in enumerate(arguments):
        if argument == "-o":
            if index + 1 == len(arguments):
                raise ValueError("-o缺少对象路径")
            result = (directory / arguments[index + 1]).resolve()
        elif argument.startswith("-o") and len(argument) > 2:
            result = (directory / argument[2:]).resolve()
    if "output" in entry:
        declared = Path(entry["output"])
        candidates = {(directory / declared).resolve(), (root / declared).resolve()}
        if result is not None and result not in candidates:
            raise ValueError("编译数据库output与命令-o不一致")
        if result is None:
            raise ValueError("当前检查要求真实-o输出路径，不能仅凭源文件猜测目标")
    if result is None:
        raise ValueError("编译行缺少对象输出路径")
    return result


def macro_values(arguments):
    values = {macro: [] for macro in MACROS}
    index = 0
    while index < len(arguments):
        argument = arguments[index]
        if argument in ("-D", "-U"):
            if index + 1 == len(arguments):
                raise ValueError(f"{argument}缺少参数")
            index += 1
            argument += arguments[index]
        if argument.startswith(("-D", "-U")):
            name, _, value = argument[2:].partition("=")
            if name in values:
                if argument.startswith("-U") or value not in ("0", "1"):
                    raise ValueError(f"{name}必须显式定义为0或1，不能取消定义")
                values[name].append(value)
        index += 1
    for macro, definitions in values.items():
        if not definitions:
            raise ValueError(f"缺少宏：{macro}")
        if len(set(definitions)) != 1:
            raise ValueError(f"重复宏冲突：{macro}={definitions}")
    return tuple(values[macro][0] for macro in MACROS)


def load_targets(root):
    reply = root / ".cmake/api/v1/reply"
    indices = list(reply.glob("index*.json"))
    if not indices:
        raise ValueError("缺少CMake File API index；配置前必须请求codemodel-v2")
    index = json.loads(max(indices, key=lambda p: (p.stat().st_mtime_ns, p.name)).read_text())
    model = json.loads((reply / index["reply"]["codemodel-v2"]["jsonFile"]).read_text())
    if len(model["configurations"]) != 1:
        raise ValueError("当前检查仅支持单配置codemodel")
    targets = {item["id"]: json.loads((reply / item["jsonFile"]).read_text())
               for item in model["configurations"][0]["targets"]}
    return targets, Path(model["paths"]["source"]).resolve()


def has_osal_inputs(target):
    """仅识别实际OSAL配置或头路径参与者，第三方GTest等不要求OSAL宏。"""
    if target["name"] == "dp_osal":
        return True
    osal_source = Path(__file__).resolve().parents[1] / "src"
    for group in target.get("compileGroups", ()):
        if any(item["define"].split("=", 1)[0] in MACROS for item in group.get("defines", ())):
            return True
        if any(Path(item["path"]).resolve().is_relative_to(osal_source)
               for item in group.get("includes", ())):
            return True
    return False


def check(root, requested, expected):
    targets, source_root = load_targets(root)
    by_name = {item["name"]: identity for identity, item in targets.items()}
    artifacts = {}
    for identity, target in targets.items():
        for item in target.get("artifacts", ()):
            path = (root / item["path"]).resolve()
            if path in artifacts and artifacts[path] != identity:
                raise ValueError(f"链接产物归属不唯一：{path}")
            artifacts[path] = identity
    selected = set()
    for name in requested:
        if name not in by_name:
            raise ValueError(f"codemodel没有目标：{name}")
        if targets[by_name[name]]["type"] != "EXECUTABLE":
            raise ValueError(f"当前检查要求指定最终可执行进程：{name}")
        pending = [by_name[name]]
        seen = set()
        while pending:
            identity = pending.pop()
            if identity in seen:
                continue
            seen.add(identity)
            target = targets[identity]
            if target["name"] == name or has_osal_inputs(target):
                selected.add(identity)
            # dependencies也包含生成器/纯构建顺序，甚至会覆盖同库真实link的backtrace。
            # 只匹配最终link实际列出的产物；静态传递库已在该进程的libraries碎片中展开。
            if target["type"] not in ("EXECUTABLE", "SHARED_LIBRARY", "MODULE_LIBRARY"):
                continue
            for dependency in target.get("dependencies", ()):
                item = targets.get(dependency["id"], {})
                if item.get("type") == "OBJECT_LIBRARY" and has_osal_inputs(item):
                    raise ValueError("当前检查不支持OSAL object-library角色，不能猜测对象归属")
            if "link" not in target:
                raise ValueError(f"缺少真实链接元数据：{target['name']}")
            directory = root / target["paths"]["build"]
            for fragment in target["link"].get("commandFragments", ()):
                if fragment["role"] != "libraries":
                    continue
                for argument in shlex.split(fragment["fragment"]):
                    linked = artifacts.get((directory / argument).resolve())
                    if linked is not None:
                        pending.append(linked)
    prefixes = {identity: (root / targets[identity]["paths"]["build"] /
                            "CMakeFiles" / (targets[identity]["name"] + ".dir")).resolve()
                for identity in selected}
    expected_sources = {}
    for identity in selected:
        expected_sources[identity] = Counter(
            ((root if item.get("isGenerated") else source_root) / item["path"]).resolve()
            for item in targets[identity].get("sources", ()) if "compileGroupIndex" in item)
        if not expected_sources[identity]:
            raise ValueError(f"缺少编译源元数据，不能视为通过：{targets[identity]['name']}")
    observed = {identity: Counter() for identity in selected}
    for entry in json.loads((root / "compile_commands.json").read_text()):
        directory = Path(entry["directory"])
        raw = entry.get("arguments")
        arguments = response_arguments(raw if raw is not None else shlex.split(entry["command"]), directory)
        output = output_path(arguments, entry, root)
        owners = [identity for identity, prefix in prefixes.items() if output.is_relative_to(prefix)]
        if not owners:
            continue
        if len(owners) != 1:
            raise ValueError(f"对象归属不唯一：{output}")
        owner = owners[0]
        actual = macro_values(arguments)
        if actual != expected:
            raise ValueError(f"目标{targets[owner]['name']}对象配置{actual}与要求{expected}不一致：{output}")
        observed[owner][(directory / entry["file"]).resolve()] += 1
    for identity in selected:
        if observed[identity] != expected_sources[identity]:
            raise ValueError(f"目标{targets[identity]['name']}编译源覆盖不完整或归属异常："
                             f"缺少{list((expected_sources[identity] - observed[identity]).elements())}，"
                             f"多出{list((observed[identity] - expected_sources[identity]).elements())}")
    count = sum(sum(items.values()) for items in observed.values())
    print(f"OSAL构建契约通过：{len(selected)}个关联目标，{count}个翻译单元，配置={expected}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--measurement", choices=("0", "1"), required=True)
    parser.add_argument("--targets", nargs="+", required=True)
    args = parser.parse_args()
    try:
        check(Path(args.build_dir).resolve(), args.targets, (args.measurement,))
        return 0
    except (ValueError, KeyError, OSError, TypeError) as error:
        print(f"OSAL构建契约失败：{error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
