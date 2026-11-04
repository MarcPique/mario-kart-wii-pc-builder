"""Maintainer-only, offline generation. Never publish the corpus, model or raw bank."""
import argparse
import json
import os
from pathlib import Path

from localization_helpers import corpus, TARGETS
from assemble_locales import assemble


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--workspace', type=Path, required=True,
                        help='Private builder workspace containing the extracted disc and RR pack')
    parser.add_argument('--cache', type=Path, required=True, help='Private translation-cache JSON')
    parser.add_argument('--review', type=Path, help='Private reviewed game-text overrides, applied after the automatic cache')
    parser.add_argument('--model-dir', type=Path, help='Optional local NLLB model to fill missing cache entries')
    parser.add_argument('--device', default='cuda', choices=['cuda', 'cpu'])
    parser.add_argument('--output', type=Path, required=True, help='Public patch/catalog output directory')
    parser.add_argument('--audit-dir', type=Path, required=True, help='PRIVATE audit directory; contains game text')
    args = parser.parse_args()
    os.environ['HF_HUB_OFFLINE'] = '1'
    os.environ['TOKENIZERS_PARALLELISM'] = 'false'
    if args.model_dir:
        import torch
        from transformers import AutoTokenizer, AutoModelForSeq2SeqLM
        _, _, _, _, _, jobs = corpus(args.workspace)
        translations = json.loads(args.cache.read_text(encoding='utf-8')) if args.cache.exists() else {}
        tokenizer = AutoTokenizer.from_pretrained(args.model_dir, local_files_only=True, src_lang='eng_Latn')
        dtype = torch.float16 if args.device == 'cuda' else torch.float32
        model = AutoModelForSeq2SeqLM.from_pretrained(args.model_dir, local_files_only=True, dtype=dtype).to(args.device).eval()
        torch.set_num_threads(4)
        for code, target in TARGETS.items():
            done = translations.setdefault(code, {})
            pending = sorted((text for text in jobs[code] if text not in done), key=len)
            for first in range(0, len(pending), 20):
                batch = pending[first:first + 20]
                encoded = tokenizer(batch, return_tensors='pt', padding=True).to(args.device)
                if encoded['input_ids'].shape[1] > 512:
                    raise ValueError('Translation sentence too long')
                with torch.inference_mode():
                    generated = model.generate(**encoded, forced_bos_token_id=tokenizer.convert_tokens_to_ids(target),
                                               max_length=None, max_new_tokens=256, num_beams=4, early_stopping=True)
                values = tokenizer.batch_decode(generated, skip_special_tokens=True)
                for source, destination in zip(batch, values):
                    if not destination.strip():
                        raise ValueError('Empty translation for ' + code)
                    done[source] = destination
                args.cache.parent.mkdir(parents=True, exist_ok=True)
                temporary = args.cache.with_suffix('.tmp')
                temporary.write_text(json.dumps(translations, ensure_ascii=False, indent=2), encoding='utf-8')
                temporary.replace(args.cache)
            print(code, 'cache ready', flush=True)
    assemble(args.workspace, args.cache, args.output, args.audit_dir,args.review)


if __name__ == '__main__':
    main()
