const { translate } = require('microsoft-translate-api');

async function main() {
  const target = process.argv[2];
  if (!target) throw new Error('target language code required');
  let input = '';
  for await (const chunk of process.stdin) input += chunk;
  const texts = JSON.parse(input);
  if (!Array.isArray(texts)) throw new Error('stdin must be a JSON string array');

  const output = [];
  for (let start = 0; start < texts.length; start += 50) {
    const chunk = texts.slice(start, start + 50);
    const result = await translate(chunk, 'en', target, { timeoutMs: 30000 });
    if (!Array.isArray(result) || result.length !== chunk.length) {
      throw new Error(`unexpected result length: ${result && result.length} for ${chunk.length}`);
    }
    for (const item of result) {
      const t = item && item.translations && item.translations[0] && item.translations[0].text;
      if (!t) throw new Error('translation item missing text');
      output.push(t);
    }
  }
  process.stdout.write(JSON.stringify(output));
}
main().catch(err => {
  console.error(err && err.stack ? err.stack : String(err));
  process.exit(1);
});
