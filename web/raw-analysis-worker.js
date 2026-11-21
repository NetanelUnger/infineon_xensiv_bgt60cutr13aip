import {analyzeRawFile} from './raw-analysis.js';

self.onmessage=async ({data:file})=>{
  try {
    const data=await analyzeRawFile(file,{onProgress:value=>self.postMessage({type:'progress',value})});
    self.postMessage({type:'done',data});
  } catch(error) {self.postMessage({type:'error',message:error.message||'לא ניתן לקרוא את הקובץ.'});}
};
