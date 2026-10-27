#!/usr/bin/env python3
import sys
import json
import base64
import io

def main():
    try:
        import numpy as np
        from PIL import Image
        from matplotlib.mathtext import MathTextParser
        parser = MathTextParser('agg')
    except Exception as e:
        sys.stderr.write(f"Failed to load dependencies: {e}\n")
        sys.stderr.flush()
        sys.exit(1)

    while True:
        line = sys.stdin.readline()
        if not line:
            break
        line = line.strip()
        if not line:
            continue
        try:
            req = json.loads(line)
            latex = req.get('latex', '').strip()
            color_hex = req.get('color', '#f0f3ff')
            dpi = int(req.get('dpi', 220))

            if not latex:
                resp = {'status': 'error', 'message': 'Empty latex expression'}
            else:
                latex_clean = latex
                if latex_clean.startswith('$$') and latex_clean.endswith('$$') and len(latex_clean) >= 4:
                    latex_clean = latex_clean[2:-2].strip()
                elif latex_clean.startswith('$') and latex_clean.endswith('$') and len(latex_clean) >= 2:
                    latex_clean = latex_clean[1:-1].strip()
                
                # Replace literal newlines with space unless escaped
                latex_clean = ' '.join(latex_clean.splitlines()).strip()
                latex_clean = '$' + latex_clean + '$'

                res = parser.parse(latex_clean, dpi=dpi)
                c = color_hex.lstrip('#')
                r, g, b = tuple(int(c[i:i+2], 16) for i in (0, 2, 4)) if len(c) == 6 else (240, 243, 255)

                rgba = np.zeros((res.image.shape[0], res.image.shape[1], 4), dtype=np.uint8)
                rgba[..., 0] = r
                rgba[..., 1] = g
                rgba[..., 2] = b
                rgba[..., 3] = res.image

                img = Image.fromarray(rgba)
                buf = io.BytesIO()
                img.save(buf, format='PNG')
                b64 = base64.b64encode(buf.getvalue()).decode('ascii')
                resp = {
                    'status': 'ok',
                    'width': res.image.shape[1],
                    'height': res.image.shape[0],
                    'png': b64
                }
        except Exception as e:
            resp = {'status': 'error', 'message': str(e)}

        sys.stdout.write(json.dumps(resp) + '\n')
        sys.stdout.flush()

if __name__ == '__main__':
    main()
