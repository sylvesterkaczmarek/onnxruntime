// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

import assert from 'assert';
import * as fs from 'fs';
import * as http from 'http';
import * as os from 'os';
import * as path from 'path';

const { downloadFile, downloadJson } = require('../../../script/install-utils');

describe('install-utils downloads', () => {
  let server: http.Server;
  let baseUrl: string;

  before((done) => {
    server = http.createServer((req, res) => {
      switch (req.url) {
        case '/json-start':
          res.writeHead(302, { Location: '/json-final' });
          res.end();
          break;
        case '/json-final':
          res.writeHead(200, { 'Content-Type': 'application/json' });
          res.end(JSON.stringify({ redirected: true }));
          break;
        case '/file-start':
          res.writeHead(307, { Location: '/file-final' });
          res.end();
          break;
        case '/file-final':
          res.writeHead(200, { 'Content-Type': 'application/octet-stream' });
          res.end('redirected file');
          break;
        case '/redirect-loop':
          res.writeHead(302, { Location: '/redirect-loop' });
          res.end();
          break;
        default:
          res.writeHead(404);
          res.end();
      }
    });

    server.listen(0, '127.0.0.1', () => {
      const address = server.address();
      if (!address || typeof address === 'string') {
        done(new Error('Failed to determine test server address.'));
        return;
      }
      baseUrl = `http://127.0.0.1:${address.port}`;
      done();
    });
  });

  after((done) => server.close(done));

  it('follows redirects when downloading JSON', async () => {
    const data = await downloadJson(`${baseUrl}/json-start`);
    assert.deepStrictEqual(data, { redirected: true });
  });

  it('follows redirects when downloading files', async () => {
    const tempDir = fs.mkdtempSync(path.join(os.tmpdir(), 'ort-node-redirect-'));
    const destination = path.join(tempDir, 'package.bin');

    try {
      await downloadFile(`${baseUrl}/file-start`, destination);
      assert.strictEqual(fs.readFileSync(destination, 'utf8'), 'redirected file');
    } finally {
      fs.rmSync(tempDir, { recursive: true, force: true });
    }
  });

  it('rejects redirect loops after the configured limit', async () => {
    await assert.rejects(downloadJson(`${baseUrl}/redirect-loop`), /Too many redirects/);
  });
});
