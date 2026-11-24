# アルカナム 日本語化 dinput.dll のソース

Steam 版 Arcanum（1.0.7.4、非公式パッチ UAP 2.0.2 を入れたものを含む）の文字表示を日本語にする `dinput.dll` と、その組み立てに使う道具です。

- `src/` … dinput.dll 本体（C）。Visual Studio 2022 の x86 ツールで `src\build.bat` を実行すると `build\dinput.dll` ができます。`src\build.bat test` は試験用の版（`build\test\dinput.dll`、配布しません）を作ります。
- `tools/` … Python の道具。`extract.py`（原作の文章を集める）→ `merge.py` → `make_data.py` → `make_font.py` → `src\build.bat` の順に使います。UAP の文章は、UAP の dat から取り出した mes / dlg を `work/ex_uap/` に置いて `extract.py --add-uap` で追加します。`make_zip.py` は配布用の zip を作ります。`win.py` は試験用の起動・操作の道具で、試験用の版を使います。

必要なもの: Python 3（Pillow、fontTools）、Noto Serif JP、訳の表（このリポジトリには入っていません）。
原作のファイル（Arcanum.exe、dat など）は各自で用意してください。
