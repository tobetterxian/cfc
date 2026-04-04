
import base64


def get_path(name):
    fns = {
        'index': 'web/index.html',
        'cimbar_js': 'web/cimbar_js.js',
        'cimbar_wasm': 'web/cimbar_js.wasm',
        'main_js': 'web/main.js',
        'output': 'web/cimbar_js.html',
    }
    return fns[name]


def read_file(name):
    with open(get_path(name), 'rt', encoding='utf-8') as f:
        return f.read()


def read_script(name):
    script = read_file(name)
    return '<script type="text/javascript">\n' + script + '\n</script>'


def read_wasm_data_uri():
    with open(get_path('cimbar_wasm'), 'rb') as f:
        wasm_base64 = base64.b64encode(f.read()).decode('ascii')
    return 'data:application/octet-stream;base64,' + wasm_base64


def main():
    contents = read_file('index')
    main_js = read_script('main_js')
    cimbar_js = read_file('cimbar_js')

    if '"cimbar_js.wasm"' in cimbar_js:
        cimbar_js = cimbar_js.replace('"cimbar_js.wasm"', '"' + read_wasm_data_uri() + '"')

    cimbar_js = '<script type="text/javascript">\n' + cimbar_js + '\n</script>'

    contents = contents.replace('<script src="main.js"></script>', main_js)
    contents = contents.replace('<script src="cimbar_js.js"></script>', cimbar_js)

    with open(get_path('output'), 'wt', encoding='utf-8') as f:
        f.write(contents)



if __name__ == '__main__':
    main()
