"""La configuracion no debe habilitar otro hardware ni modificar archivos si es invalida."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("configurar", ROOT / "tools/configurar_equipo.py")
configurar = importlib.util.module_from_spec(spec)
spec.loader.exec_module(configurar)


class ConfigurationTests(unittest.TestCase):
    def valid(self):
        return dict(mac_c6="02:00:00:00:00:01", mac_tft="02:00:00:00:00:02",
                    mac_c5="02:00:00:00:00:03", mac_obd_ble="",
                    placa_relevadores=configurar.BOARD, habilitar_relevadores=False)

    def test_duplicate_and_invalid_identities(self):
        for value in ("", "00:00:00:00:00:00", "ff:ff:ff:ff:ff:ff", "01:02:03:04:05:06", "02:11:22:ff:fe:33:44:55"):
            config = self.valid()
            config["mac_c6"] = value
            with self.assertRaises(ValueError):
                configurar.render(config)
        config = self.valid()
        config["mac_c5"] = config["mac_c6"]
        with self.assertRaises(ValueError):
            configurar.render(config)

    def test_wrong_board_and_non_boolean_enable(self):
        for changes in ({"placa_relevadores": "otra placa", "habilitar_relevadores": True},
                        {"habilitar_relevadores": "false"}, {"mac_obd_ble": 'a";malformed'},
                        {"mac_obd_ble": None}):
            with self.assertRaises(ValueError):
                configurar.render({**self.valid(), **changes})

    def test_validate_all_before_writing_and_keep_outputs_blocked(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            for folder in configurar.FOLDERS:
                (root / folder).mkdir()
                (root / folder / "Configuracion_Radio.h").write_text("original")
            board = root / "XJ_Relevadores_C5/Configuracion_Placa.h"
            board.write_text("constexpr bool configuracionHabilitada = false;\n")
            files = [*root.glob("*/Configuracion_Radio.h"), board]
            original = {f: f.read_bytes() for f in files}
            with self.assertRaises(ValueError):
                configurar.apply({**self.valid(), "mac_c5": ""}, root)
            configurar.apply(self.valid(), root, dry_run=True)
            self.assertEqual(original, {f: f.read_bytes() for f in files})
            self.assertFalse(configurar.apply(self.valid(), root))
            self.assertEqual(len({f.read_bytes() for f in root.glob("*/Configuracion_Radio.h")}), 1)
            self.assertIn("= false;", board.read_text())
            self.assertTrue(configurar.apply({**self.valid(), "habilitar_relevadores": True}, root))
            self.assertIn("= true;", board.read_text())


if __name__ == "__main__":
    unittest.main()
