"""构建契约负例：目标归属、响应文件、缺宏及跨进程隔离。"""
import json
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile
import unittest

SCRIPT = Path(__file__).with_name("check_build_contract.py")


class BuildContractTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.reply = self.root / ".cmake/api/v1/reply"
        self.reply.mkdir(parents=True)
        self.targets = []
        self.commands = []

    def target(self, name, measurement="0", dependencies=(), arguments=False,
               kind=None, linked=None):
        kind = kind or ("STATIC_LIBRARY" if name == "dp_osal" else "EXECUTABLE")
        artifact = "lib" + name + ".a" if kind == "STATIC_LIBRARY" else name
        target = {"id": name, "name": name, "type": kind,
                  "paths": {"build": ".", "source": "."},
                  "artifacts": [{"path": artifact}],
                  "compileGroups": [{"defines": [{"define": m + "=" + v} for m, v in zip(
                      ("DP_OSAL_ENABLE_MEASUREMENT",),
                      (measurement,))]}],
                  "sources": [{"path": "same.cpp", "compileGroupIndex": 0}],
                  "link": {"commandFragments": [{"fragment": "lib" + x + ".a", "role": "libraries"}
                                                   for x in (dependencies if linked is None else linked)]},
                  "dependencies": [{"id": x} for x in dependencies]}
        self.targets.append({"id": name, "name": name, "jsonFile": name + ".json"})
        (self.reply / (name + ".json")).write_text(json.dumps(target))
        argv = ["c++", "-DDP_OSAL_ENABLE_MEASUREMENT=" + measurement,
                "-o", f"CMakeFiles/{name}.dir/same.cpp.o", "-c", "same.cpp"]
        entry = {"directory": str(self.root), "file": "same.cpp"}
        entry["arguments" if arguments else "command"] = argv if arguments else shlex.join(argv)
        self.commands.append(entry)
        return entry

    def run_check(self, *targets, measurement="0"):
        (self.reply / "codemodel.json").write_text(json.dumps({"paths": {"source": str(self.root), "build": str(self.root)},
                                                              "configurations": [{"targets": self.targets}]}))
        (self.reply / "index.json").write_text(json.dumps({"reply": {"codemodel-v2": {"jsonFile": "codemodel.json"}}}))
        (self.root / "compile_commands.json").write_text(json.dumps(self.commands))
        return subprocess.run([sys.executable, str(SCRIPT), "--build-dir", str(self.root),
                               "--measurement", measurement,
                               "--targets", *targets], capture_output=True, text=True, check=False)

    def test_valid_process_and_independent_process_may_differ(self):
        self.target("app", dependencies=("dp_osal",))
        self.target("dp_osal", arguments=True)
        self.target("other", "1")
        self.assertEqual(self.run_check("app").returncode, 0)
        self.assertEqual(self.run_check("other", measurement="1").returncode, 0)

    def test_missing_macro_rejected(self):
        row = self.target("app", arguments=True)
        row["arguments"] = [x for x in row["arguments"] if not x.startswith("-DDP_OSAL_ENABLE_MEASUREMENT")]
        self.assertNotEqual(self.run_check("app").returncode, 0)

    def test_conflicting_duplicate_macro_rejected(self):
        row = self.target("app", arguments=True)
        row["arguments"].insert(1, "-DDP_OSAL_ENABLE_MEASUREMENT=1")
        self.assertNotEqual(self.run_check("app").returncode, 0)

    def test_same_process_dependency_mismatch_rejected(self):
        self.target("app", dependencies=("dp_osal",))
        self.target("dp_osal", "1")
        self.assertNotEqual(self.run_check("app").returncode, 0)

    def test_response_file_and_object_ownership(self):
        row = self.target("app", arguments=True)
        (self.root / "flags.rsp").write_text("-DDP_OSAL_ENABLE_MEASUREMENT=0")
        row["arguments"] = ["c++", "@flags.rsp", "-o", "CMakeFiles/app.dir/same.cpp.o", "-c", "same.cpp"]
        self.target("other", "1")
        self.assertEqual(self.run_check("app").returncode, 0)
        row["arguments"][3] = "CMakeFiles/unknown.dir/same.cpp.o"
        self.assertNotEqual(self.run_check("app").returncode, 0)

    def test_response_cycle_and_undefined_macro_rejected(self):
        row = self.target("app", arguments=True)
        row["arguments"].insert(1, "@cycle.rsp")
        (self.root / "cycle.rsp").write_text("@cycle.rsp")
        self.assertNotEqual(self.run_check("app").returncode, 0)
        row["arguments"][1] = "-UDP_OSAL_ENABLE_MEASUREMENT"
        self.assertNotEqual(self.run_check("app").returncode, 0)

    def test_build_only_dependencies_are_separate_processes(self):
        self.target("app", dependencies=("dp_osal", "generator", "build_only", "both", "common"),
                    linked=("dp_osal", "both", "common"))
        self.target("dp_osal")
        self.target("generator", "1")
        self.target("build_only", "1", kind="STATIC_LIBRARY")
        self.target("both", kind="STATIC_LIBRARY")
        self.target("common", kind="STATIC_LIBRARY")
        self.assertEqual(self.run_check("app").returncode, 0)

    def test_partial_object_ownership_cannot_hide_translation_unit(self):
        self.target("app")
        path = self.reply / "app.json"
        target = json.loads(path.read_text())
        target["sources"].append({"path": "second.cpp", "compileGroupIndex": 0})
        path.write_text(json.dumps(target))
        self.commands.append({"directory": str(self.root), "file": "second.cpp", "arguments": [
            "c++", "-DDP_OSAL_ENABLE_MEASUREMENT=1",
            "-o", "CMakeFiles/unknown.dir/second.cpp.o", "-c", "second.cpp"]})
        self.assertNotEqual(self.run_check("app").returncode, 0)

    def test_missing_translation_unit_rejected(self):
        self.target("app")
        path = self.reply / "app.json"
        target = json.loads(path.read_text())
        target["sources"].append({"path": "second.cpp", "compileGroupIndex": 0})
        path.write_text(json.dumps(target))
        self.assertNotEqual(self.run_check("app").returncode, 0)


if __name__ == "__main__":
    unittest.main()
