//#include "RAMPface.h"
#include "mzParser.h"

int checkFileType(const char* fname){
	char file[256];
	char ext[256];
	char *tok;
	char preExt[256];
	unsigned int i;

	strcpy(ext,"");

	strcpy(file,fname);
	tok=strtok(file,".\n");
	while(tok!=NULL){
		strcpy(preExt,ext);
		strcpy(ext,tok);
		tok=strtok(NULL,".\n");
	}

	for(i=0;i<strlen(ext);i++) ext[i]=toupper(ext[i]);
	for(i=0;i<strlen(preExt);i++) preExt[i]=toupper(preExt[i]);

	if(!strcmp(ext,"MZML")) return 1;
	if(!strcmp(ext,"MZXML")) return 2;
	if(!strcmp(ext,"GZ")) {
		if(!strcmp(preExt,"MZML")) return 3;
		if(!strcmp(preExt,"MZXML")) return 4;
		cerr << "Unknown .gz file. Only .mzML.gz and .mzXML.gz allowed. No file loaded." << endl;
		return 0;
	}
	cerr << "Unknown file type. No file loaded." << endl;
	return 0;
}

ramp_fileoffset_t getIndexOffset(RAMPFILE *pFI){
	switch(pFI->fileType){
		case 1:
		case 3:
			return (ramp_fileoffset_t)pFI->mzML->getIndexOffset();
			break;
		case 2:
		case 4:
			return (ramp_fileoffset_t)pFI->mzXML->getIndexOffset();
			break;
		default:
			return -1;
	}
}

void getScanSpanRange(const struct ScanHeaderStruct *scanHeader, int *startScanNum, int *endScanNum) {
   if (0 == scanHeader->mergedResultStartScanNum || 0 == scanHeader->mergedResultEndScanNum) {
      *startScanNum = scanHeader->acquisitionNum;
      *endScanNum = scanHeader->acquisitionNum;
   } else {
      *startScanNum = scanHeader->mergedResultStartScanNum;
      *endScanNum = scanHeader->mergedResultEndScanNum;
   }
}

void rampCloseFile(RAMPFILE *pFI){
	if(pFI!=NULL) {
		delete pFI;
		pFI=NULL;
	}
}

string rampConstructInputFileName(const string &basename){
	int len;
	char *buf = new char[len = (int)(basename.length()+100)]; 
	rampConstructInputPath(buf, len, "", basename.c_str());
	string result(buf);
	delete[] buf;
	return result;
}

char* rampConstructInputFileName(char *buf,int buflen,const char *basename){
	return rampConstructInputPath(buf, buflen, "", basename);
}

char* rampConstructInputPath(char *buf, int inbuflen, const char *dir_in, const char *basename){

	if( (int)(strlen(dir_in)+strlen(basename)+1) > inbuflen ){
		printf("rampConstructInputPath error: buffer too small for file\n");
		return NULL;
	}

	FILE* f;
	char *result = NULL;

	//Try opening the base name first, then with directory:
	for(int j=0;j<2;j++){
		for(int i=0;i<4;i++){

			if(j==1){
				strcpy(buf,dir_in);
				strcat(buf,"/");
				strcat(buf,basename);
			} else {
				strcpy(buf,basename);
			}

			switch(i){
				case 0:	strcat(buf,".mzML");	break;
				case 1:	strcat(buf,".mzXML"); break;
				case 2:	strcat(buf,".mzML.gz"); break;
				case 3:	strcat(buf,".mzXML.gz");	break;
				default: break;
			}
			
			f=fopen(buf,"r");
			if(f==NULL) continue;
				
			fclose(f);
			result=buf;
			return result;
		}
	}

	printf("rampConstructInputPath: file not found\n");
	strcpy(buf,"");
	result=NULL;
	return result;

}


const char** rampListSupportedFileTypes(){
	if (!data_Ext.size()) { // needs init
		data_Ext.push_back(".mzXML");
		data_Ext.push_back(".mzML");
		data_Ext.push_back(".mzXML.gz");
		data_Ext.push_back(".mzML.gz");
		data_Ext.push_back(NULL); // end of list
	}
	return &(data_Ext[0]);
}

RAMPFILE* rampOpenFile(const char* filename){
	int i=checkFileType(filename);
	if(i==0){
		return NULL;
	} else {
		RAMPFILE* r=new RAMPFILE();
		r->bs = new BasicSpectrum();
		r->fileType=i;
		switch(i){
			case 1: //mzML
			case 3:
				r->mzML=new SAXMzmlHandler(r->bs);
				if(i==3)r->mzML->setGZCompression(true);
				else r->mzML->setGZCompression(false);
				if(!r->mzML->load(filename)){
					delete r;
					return NULL;
				} else {
					return r;
				}
			case 2: //mzXML
			case 4:
				r->mzXML=new SAXMzxmlHandler(r->bs);
				if(i==4) r->mzXML->setGZCompression(true);
				else r->mzXML->setGZCompression(false);
				if(!r->mzXML->load(filename)){
					delete r;
					return NULL;
				} else {
					return r;
				}
			default:
				delete r;
				return NULL;
		}
	}

}

char* rampValidFileType(const char *buf){
	char ext[256];
	const char* result = strrchr(buf,'.');

	strcpy(ext,result);
	for(unsigned int i=0;i<strlen(ext);i++) ext[i]=toupper(ext[i]);

	if(!strcmp(ext,".MZML")) return (char*) result;
	if(!strcmp(ext,".MZXML")) return (char*) result;
	if(!strcmp(ext,".GZ")) {
		cout << ".gz is not allowed. Please deflate your data. No file loaded." << endl;
	}
	if(!strcmp(ext,".MZDATA")) {
		cout << ".mzData is not supported. Please convert to mzXML or mzML." << endl;
	}
	result=NULL;
	return (char*) result;
}

//MH: Read header is redundant with readPeaks, which automatically reads the header.
//But due to legacy issues, this function must exist.
void readHeader(RAMPFILE *pFI, ramp_fileoffset_t lScanIndex, struct ScanHeaderStruct *scanHeader){

	vector<cindex>* v;
	unsigned int i;

	//memset(scanHeader,0,sizeof(struct ScanHeaderStruct));
	scanHeader->acquisitionNum=-1;
	scanHeader->activationMethod[0]='\0';
	scanHeader->basePeakIntensity=0.0;
	scanHeader->basePeakMZ=0.0;
	scanHeader->collisionEnergy=0.0;
	scanHeader->filePosition=0;
	scanHeader->highMZ=0.0;
	scanHeader->ionisationEnergy=0.0;
	scanHeader->lowMZ=0.0;
	scanHeader->msLevel=0;
	scanHeader->precursorCharge=0;
	scanHeader->precursorIntensity=0.0;
	scanHeader->precursorMZ=0.0;
	scanHeader->precursorScanNum=-1;
	scanHeader->retentionTime=0.0;
	scanHeader->totIonCurrent=0.0;

	if(lScanIndex<0) return;
	
	switch(pFI->fileType){
		case 1:
		case 3:
			v=pFI->mzML->getIndex();
			for(i=0;i<v->size();i++) {
				if(v->at(i).offset==(f_off)lScanIndex) {
					if(!pFI->mzML->readHeader(v->at(i).scanNum)){
						v=NULL;
						return;
					}
					break;
				}
			}
			break;
		case 2:
		case 4:
			v=pFI->mzXML->getIndex();
			for(i=0;i<v->size();i++) {
				if(v->at(i).offset==(f_off)lScanIndex) {
					if(!pFI->mzXML->readHeader(v->at(i).scanNum)){
						v=NULL;
						return;
					}
					break;
				}
			}
			break;
		default:
			pFI->bs->clear();
			v=NULL;
			return;
	}
	v=NULL;

	scanHeader->acquisitionNum=pFI->bs->getScanNum();
	scanHeader->basePeakIntensity=pFI->bs->getBasePeakIntensity();
	scanHeader->basePeakMZ=pFI->bs->getBasePeakMZ();
	scanHeader->collisionEnergy=pFI->bs->getCollisionEnergy();
	scanHeader->highMZ=pFI->bs->getHighMZ();
	scanHeader->lowMZ=pFI->bs->getLowMZ();
	scanHeader->msLevel=pFI->bs->getMSLevel();
	scanHeader->peaksCount=pFI->bs->getPeaksCount();
	scanHeader->precursorCharge=pFI->bs->getPrecursorCharge();
	scanHeader->precursorIntensity=pFI->bs->getPrecursorIntensity();
	scanHeader->compensationVoltage=pFI->bs->getCompensationVoltage();
	scanHeader->precursorMZ=pFI->bs->getPrecursorMZ();
	scanHeader->precursorScanNum=pFI->bs->getPrecursorScanNum();
	scanHeader->retentionTime=(double)pFI->bs->getRTime(false);
	scanHeader->totIonCurrent=pFI->bs->getTotalIonCurrent();

	switch(pFI->bs->getActivation()){
		case 1: strcpy(scanHeader->activationMethod,"CID"); break;
		case 2: strcpy(scanHeader->activationMethod,"HCD"); break;
		case 3: strcpy(scanHeader->activationMethod,"ETD"); break;
		case 4: strcpy(scanHeader->activationMethod,"ETD+SA"); break;
		case 5: strcpy(scanHeader->activationMethod,"ECD"); break;
		default: strcpy(scanHeader->activationMethod,""); break;
	}

}

//MH: Indexes in RAMP are stored in an array indexed by scan number, with -1 for the offset
//if the scan number does not exist.
ramp_fileoffset_t* readIndex(RAMPFILE *pFI, ramp_fileoffset_t indexOffset, int *iLastScan){
	vector<cindex>* v;
	ramp_fileoffset_t* rIndex;
	unsigned int i;
	switch(pFI->fileType){
		case 1:
		case 3:
			v=pFI->mzML->getIndex();
			rIndex = (ramp_fileoffset_t *) malloc((pFI->mzML->highScan()+2)*sizeof(ramp_fileoffset_t));
			memset(rIndex,-1,(pFI->mzML->highScan()+2)*sizeof(ramp_fileoffset_t));
			for(i=0;i<v->size();i++) rIndex[v->at(i).scanNum]=(ramp_fileoffset_t)v->at(i).offset;
			rIndex[i]=-1;
			*iLastScan=v->at(i-1).scanNum;
			break;
		case 2:
		case 4:
			v=pFI->mzXML->getIndex();
			//cout << v->size() << endl;
			//cout << pFI->mzXML->highScan() << endl;
			rIndex = (ramp_fileoffset_t *) malloc((pFI->mzXML->highScan()+2)*sizeof(ramp_fileoffset_t));
			memset(rIndex,-1,(pFI->mzXML->highScan()+2)*sizeof(ramp_fileoffset_t));
			for(i=0;i<v->size();i++) rIndex[v->at(i).scanNum]=(ramp_fileoffset_t)v->at(i).offset;
			rIndex[i]=-1;
			*iLastScan=v->at(i-1).scanNum;
			break;
		default:
			rIndex=NULL;
			*iLastScan=0;
			break;
	}
	v=NULL;
	return rIndex;
}

void readMSRun(RAMPFILE *pFI, struct RunHeaderStruct *runHeader){

	vector<cindex>* v;

	//memset(scanHeader,0,sizeof(struct ScanHeaderStruct));
	runHeader->dEndTime=0.0;
	runHeader->dStartTime=0.0;
	runHeader->endMZ=0.0;
	runHeader->highMZ=0.0;
	runHeader->lowMZ=0.0;
	runHeader->scanCount=0;
	runHeader->startMZ=0.0;
	
	switch(pFI->fileType){
		case 1:
		case 3:
			v=pFI->mzML->getIndex();
			runHeader->scanCount=v->size();
			pFI->mzML->readHeader(v->at(0).scanNum);
			runHeader->dStartTime=pFI->bs->getRTime(false);
			pFI->mzML->readHeader(v->at(v->size()-1).scanNum);
			runHeader->dEndTime=pFI->bs->getRTime(false);
			pFI->bs->clear();
			v=NULL;
			break;
		case 2:
		case 4:
			v=pFI->mzXML->getIndex();
			runHeader->scanCount=v->size();
			pFI->mzXML->readHeader(v->at(0).scanNum);
			runHeader->dStartTime=pFI->bs->getRTime(false);
			pFI->mzXML->readHeader(v->at(v->size()-1).scanNum);
			runHeader->dEndTime=pFI->bs->getRTime(false);
			pFI->bs->clear();
			v=NULL;
			break;
		default:
			break;
	}

}

//MH: Matching the index is very indirect, but requires less code,
//making this wrapper much easier to read
RAMPREAL* readPeaks(RAMPFILE* pFI, ramp_fileoffset_t lScanIndex){
	vector<cindex>* v;
	unsigned int i;
	RAMPREAL* pPeaks=NULL;

	if(lScanIndex<0) return pPeaks;

	switch(pFI->fileType){
		case 1:
		case 3:
			v=pFI->mzML->getIndex();
			for(i=0;i<v->size();i++) {
				if(v->at(i).offset==(f_off)lScanIndex) {
					pFI->mzML->readSpectrum(v->at(i).scanNum);
					break;
				}
			}
			break;
		case 2:
		case 4:
			v=pFI->mzXML->getIndex();
			for(i=0;i<v->size();i++) {
				if(v->at(i).offset==(f_off)lScanIndex) {
					pFI->mzXML->readSpectrum(v->at(i).scanNum);
					break;
				}
			}
			break;
		default:
			pFI->bs->clear();
			break;
	}
	v=NULL;


	unsigned int j=0;
	if(pFI->bs->size()>0){
		pPeaks = (RAMPREAL *) malloc((pFI->bs->size()+1) * 2 * sizeof(RAMPREAL) + 1);
		for(i=0;i<pFI->bs->size();i++){
			pPeaks[j++]=pFI->bs->operator [](i).mz;
			pPeaks[j++]=pFI->bs->operator [](i).intensity;
		}
	} else {
		pPeaks = (RAMPREAL *) malloc(2 * sizeof(RAMPREAL));
	}
	pPeaks[j]=-1;

	return pPeaks;
}

int readPeaksCount(RAMPFILE *pFI, ramp_fileoffset_t lScanIndex){
	ScanHeaderStruct s;
	readHeader(pFI, lScanIndex, &s);
	return s.peaksCount;
}

void readRunHeader(RAMPFILE *pFI, ramp_fileoffset_t *pScanIndex, struct RunHeaderStruct *runHeader, int iLastScan){
	vector<cindex>* v;
	unsigned int i;

	runHeader->scanCount=0;
	runHeader->dEndTime=0.0;
	runHeader->dStartTime=0.0;
	runHeader->endMZ=0.0;
	runHeader->highMZ=0.0;
  runHeader->lowMZ=0.0;
  runHeader->startMZ=0.0;
	
	switch(pFI->fileType){
		case 1:
		case 3:
			v=pFI->mzML->getIndex();
			runHeader->scanCount=v->size();
			
			pFI->mzML->readHeader(v->at(0).scanNum);
			runHeader->dStartTime=(double)pFI->bs->getRTime(false);
			runHeader->lowMZ=pFI->bs->getLowMZ();
			runHeader->highMZ=pFI->bs->getHighMZ();
			runHeader->startMZ=runHeader->lowMZ;
			runHeader->endMZ=runHeader->highMZ;
			
			for(i=1;i<v->size();i++) {
				pFI->mzML->readHeader(v->at(i).scanNum);
				if(pFI->bs->getLowMZ()<runHeader->lowMZ) {
					runHeader->lowMZ=pFI->bs->getLowMZ();
					runHeader->startMZ=runHeader->lowMZ;
				}
				if(pFI->bs->getHighMZ()>runHeader->highMZ){
					runHeader->highMZ=pFI->bs->getHighMZ();
					runHeader->endMZ=runHeader->highMZ;
				}
			}
			pFI->mzML->readHeader(v->at(v->size()-1).scanNum);
			break;

		case 2:
		case 4:
			v=pFI->mzXML->getIndex();
			runHeader->scanCount=v->size();
			
			pFI->mzXML->readHeader(v->at(0).scanNum);
			runHeader->dStartTime=(double)pFI->bs->getRTime(false);
			runHeader->lowMZ=pFI->bs->getLowMZ();
			runHeader->highMZ=pFI->bs->getHighMZ();
			runHeader->startMZ=runHeader->lowMZ;
			runHeader->endMZ=runHeader->highMZ;
			
			for(i=1;i<v->size();i++) {
				pFI->mzXML->readHeader(v->at(i).scanNum);
				if(pFI->bs->getLowMZ()<runHeader->lowMZ) {
					runHeader->lowMZ=pFI->bs->getLowMZ();
					runHeader->startMZ=runHeader->lowMZ;
				}
				if(pFI->bs->getHighMZ()>runHeader->highMZ){
					runHeader->highMZ=pFI->bs->getHighMZ();
					runHeader->endMZ=runHeader->highMZ;
				}
			}
			pFI->mzXML->readHeader(v->at(v->size()-1).scanNum);
			break;

		default:
			pFI->bs->clear();
			v=NULL;
			return;
	}
	v=NULL;

}




//--------------------------------------------------
// CACHED RAMP FUNCTIONS
//--------------------------------------------------
void clearScanCache(struct ScanCacheStruct* cache){
	for (int i=0; i<cache->size; i++) {
		if (cache->peaks[i] == NULL) continue;
		free(cache->peaks[i]);
		cache->peaks[i] = NULL;
	}
	memset(cache->headers, 0, cache->size * sizeof(struct ScanHeaderStruct));
}

void freeScanCache(struct ScanCacheStruct* cache){
	if (cache) {
		for (int i=0; i<cache->size; i++){
			if (cache->peaks[i] != NULL) free(cache->peaks[i]);
    }
		free(cache->peaks);
		free(cache->headers);
		free(cache);
	}
}

int getCacheIndex(struct ScanCacheStruct* cache, int seqNum) {
	int seqNumStart = cache->seqNumStart;
	int size = cache->size;

	// First access, just set the start to seqNum.
	if (seqNumStart == 0) cache->seqNumStart = seqNum;

	// If requested scan is less than cache start, shift cache window
	// left to start at requested scan.
	else if (seqNum < seqNumStart) shiftScanCache(cache, (int) (seqNum - seqNumStart));

	// If requested scan is greater than cache end, shift cache window
	// right so last entry is requested scan.
	else if (seqNum >= seqNumStart + size) shiftScanCache(cache, (int) (seqNum - (seqNumStart + size - 1)));

	return (int) (seqNum - cache->seqNumStart);
}

struct ScanCacheStruct* getScanCache(int size){
	struct ScanCacheStruct* cache = (struct ScanCacheStruct*) malloc(sizeof(struct ScanCacheStruct));
	cache->seqNumStart = 0;
	cache->size = size;
	cache->headers = (struct ScanHeaderStruct*) calloc(size, sizeof(struct ScanHeaderStruct));
	cache->peaks = (RAMPREAL**) calloc(size, sizeof(RAMPREAL*));
	return cache;
}

const struct ScanHeaderStruct* readHeaderCached(struct ScanCacheStruct* cache, int seqNum, RAMPFILE* pFI, ramp_fileoffset_t lScanIndex){
	int i = getCacheIndex(cache, seqNum);
	if (cache->headers[i].msLevel == 0) readHeader(pFI, lScanIndex, cache->headers + i);
	return cache->headers + i;
}

int	readMsLevelCached(struct ScanCacheStruct* cache, int seqNum, RAMPFILE* pFI, ramp_fileoffset_t lScanIndex){
	const struct ScanHeaderStruct* header = readHeaderCached(cache, seqNum, pFI, lScanIndex);
	return header->msLevel;
}

const RAMPREAL* readPeaksCached(struct ScanCacheStruct* cache, int seqNum, RAMPFILE* pFI, ramp_fileoffset_t lScanIndex){
	int i = getCacheIndex(cache, seqNum);
	if (cache->peaks[i] == NULL) cache->peaks[i] = readPeaks(pFI, lScanIndex);
	return cache->peaks[i];
}

void shiftScanCache(struct ScanCacheStruct* cache, int nScans) {
	int i;
	cache->seqNumStart += nScans;
	if (abs(nScans) > cache->size) {
		// If the shift is larger than the size of the cache window,
		// just clear the whole cache.
		clearScanCache(cache);
	} else if (nScans > 0) {
		// Shifting window to the right.  Memory moves right, with new
		// empty scans on the end.

		// Free the peaks that memmove will overwrite.
		for (i = 0; i < nScans; i++) {
			if (cache->peaks[i] != NULL) free(cache->peaks[i]);
		}
		memmove(cache->peaks, cache->peaks + nScans, (cache->size - nScans) * sizeof(RAMPREAL*));
		memset(cache->peaks + cache->size - nScans, 0, nScans * sizeof(RAMPREAL*));
		memmove(cache->headers, cache->headers + nScans,(cache->size - nScans) * sizeof(struct ScanHeaderStruct));
		memset(cache->headers + cache->size - nScans, 0, nScans * sizeof(struct ScanHeaderStruct));
	} else if (nScans < 0) {
		// Shifting window to the left.  Memory moves right, with new
		// empty scans at the beginning.
		nScans = -nScans;

		// Free the peaks that memmove will overwrite.
		for (i = 0; i < nScans; i++) {
			if (cache->peaks[cache->size - 1 - i] != NULL) free(cache->peaks[cache->size - 1 - i]);
		}
		memmove(cache->peaks + nScans, cache->peaks, (cache->size - nScans) * sizeof(RAMPREAL*));
		memset(cache->peaks, 0, nScans * sizeof(RAMPREAL*));
		memmove(cache->headers  + nScans, cache->headers, (cache->size - nScans) * sizeof(struct ScanHeaderStruct));
		memset(cache->headers, 0, nScans * sizeof(struct ScanHeaderStruct));
	}
}



//--------------------------------------------------
// DEAD FUNCTIONS
//--------------------------------------------------
InstrumentStruct* getInstrumentStruct(RAMPFILE *pFI){
	cerr << "call to unsupported function: getInstrumentStruct(RAMPFILE *pFI)" << endl;
	return NULL;
}

int isScanAveraged(struct ScanHeaderStruct *scanHeader){
	cerr << "call to unsupported function: isScanAveraged(struct ScanHeaderStruct *scanHeader)" << endl;
	return 0;
}

int isScanMergedResult(struct ScanHeaderStruct *scanHeader){
	cerr << "call to unsupported function: isScanMergedResult(struct ScanHeaderStruct *scanHeader)" << endl;
	return 0;
}

int rampSelfTest(char *filename){
	cerr << "call to unsupported function: rampSelfTest(char *filename)" << endl;
	return 0;
}

char* rampTrimBaseName(char *buf){
	cerr << "call to unsupported function: rampTrimBaseName(char *buf)" << endl;
	return "";
}

int rampValidateOrDeriveInputFilename(char *inbuf, int inbuflen, char *spectrumName){
	cerr << "call to unsupported function: rampValidateOrDeriveInputFilename(char *inbuf, int inbuflen, char *spectrumName)" << endl;
	return 0;
}

int readMsLevel(RAMPFILE *pFI, ramp_fileoffset_t lScanIndex){
	cerr << "call to unsupported function: readMsLevel(RAMPFILE *pFI, ramp_fileoffset_t lScanIndex)" << endl;
	return 0;
}

double readStartMz(RAMPFILE *pFI, ramp_fileoffset_t lScanIndex){
	cerr << "call to unsupported function: readStartMz(RAMPFILE *pFI, ramp_fileoffset_t lScanIndex)" << endl;
	return 0.0;
}

double readEndMz(RAMPFILE *pFI, ramp_fileoffset_t lScanIndex){
	cerr << "call to unsupported function: readEndMz(RAMPFILE *pFI, ramp_fileoffset_t lScanIndex)" << endl;
	return 0.0;
}

void setRampOption(long option){
	cerr << "call to unsupported function: setRampOption(long option)" << endl;
}
