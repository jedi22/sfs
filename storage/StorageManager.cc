#include "StorageManager.h"

namespace fs = boost::filesystem;

StorageManager::StorageManager(std::string rpath){
	path = rpath;
	getAllStegPieces(false);
	computeSize();
}

StorageManager::~StorageManager(){
	// Free ALL the things
	for (auto * component : components){
		delete component;
	}
}

void StorageManager::computeSize(){
	size = 0;
	stegSize = 0;
	for(const auto &component : components){
		size += component->getSize();
		stegSize += component->getStegSize();
	}
}

void StorageManager::organizeFiles(std::vector<std::string>& files){
	// Right now we just use alphabetical order but eventually,
	// this should be a random order based on a password.
	std::sort(files.begin(), files.end());
}

void StorageManager::getAllStegPieces(bool recurse){
	fs::path p (path);
	fs::directory_iterator end_itr;
	std::vector<std::string> files;

	for (fs::directory_iterator itr(p); itr != end_itr; ++itr){
		// Is directory check, just use 1 level of files for now
		if (is_regular_file(itr->path())) {
			files.push_back(itr->path().string());
		}
	}
	// Put the files into the right order
	organizeFiles(files);

	for(const auto &file : files){
		// This needs to be turned into a registry somehow
		if(fs::extension(file) == ".jpg"){
			components.push_back(new Jpeg(file));
		}
	}
}

void StorageManager::printStegPieces(){
	std::cout << "These files, in this order, make up your sFS\n";
	std::string name;
	for( auto* component : components){
		name = component->name;
		if(component->name.length() > 5){
			name = component->name.substr(0,5);
		}
		std::cout << name << "\tType: " << component->getStegType() << "\t Size: " << component->getSize() << "\t StegSize: " << component->getStegSize() << "\n";
	}
	std::string unit = "bytes";
	double adjustedUnitSize = 0.0;
	if (stegSize > 1048576){
		unit = "MiB";
		adjustedUnitSize = stegSize/1048576.0;
	}else if(stegSize > 1024){
		unit = "KiB";
		adjustedUnitSize = stegSize/1024.0;
	}
	std::cout << "The sFS system has " << adjustedUnitSize << " " << unit << "\n";
	std::cout << "The sFS system has approximately " << ((float)stegSize)/((float)size)*100 << "% efficiency\n";
}

// Get the size that all the files are taking on disk
size_t StorageManager::getApparentSize(){
  return size;
}

// Get the size of the virtual steganographic disk
size_t StorageManager::getStegSize(){
  return stegSize;
}

// Write length bytes of data to location in the steg disk
int StorageManager::write(const void* rdata, int location, int length){
	const char* data = (char*)rdata;
	int bytesToGo = location;
	//Find file
	int fileIndex = 0;
	for( auto* component : components){
		if(bytesToGo > component->getStegSize()){
			bytesToGo -= component->getStegSize();
			fileIndex++;
		}else{
			break;
		}
	}
	// Check if there is enough space to the end of the disk
	// Space left in this file
	size_t spaceLeft = length - (components[fileIndex]->getStegSize()-bytesToGo);
	for(int i=fileIndex+1; i<components.size() && spaceLeft < length; ++i){
		spaceLeft += components[i]->getStegSize();
	}

	if(spaceLeft < length){
		return HIT_DISK_END;
	}
	// Here we can be assued there is enough space in the disk to store the data
	// Loop through all needed files and do the writes

	//TODO: In N writes all writes but 1 succeed, we cannot undo and the filesystem
	//will be corrupted. This should be fixed by writing out to new files, then when
	//they all succeed, move the old file to tmp, move the new ones in, then delete the old
	//ones. This way, a failed write on one file, will not corrupt the whole disk. Expensive?
	size_t bytesWritten = 0;
	int status = 0;
	while(bytesWritten < length){
		// How much data can be written into this steg piece
		int writeLen = std::min(components[fileIndex]->getStegSize()-bytesToGo, length-bytesWritten);
		status = components[fileIndex]->write(data+bytesWritten, bytesToGo, writeLen);
		if(status != SUCCESS){
			// Currently, if we get here, the filesystem is now probably corrupted
			std::cout << "Writing to " <<  components[fileIndex]->name << " failed.\n";
			std::cout << "The filesystem may now be corrupt.\nSorry.\n";
			return status;
		}
		fileIndex++;
		bytesToGo = 0;
		bytesWritten += writeLen;
	}

  return SUCCESS;
}

// Read length bytes of data from location in the steg disk
int StorageManager::read(void* rdata, int location, int length){
	char* data = (char*)rdata;
	memset(data ,0, length);
	int bytesToGo = location;
	//Find file
	int fileIndex = 0;
	for( auto* component : components){
		if(bytesToGo > component->getStegSize()){
			bytesToGo -= component->getStegSize();
			fileIndex++;
		}else{
			break;
		}
	}
	// Check if there is enough space to the end of the disk
	// Space left in this file
	size_t spaceLeft = length - (components[fileIndex]->getStegSize()-bytesToGo);
	for(int i=fileIndex+1; i<components.size() && spaceLeft < length; ++i){
		spaceLeft += components[i]->getStegSize();
	}

	if(spaceLeft < length){
		return HIT_DISK_END;
	}

	size_t bytesRead = 0;
	int status = 0;
	while(bytesRead < length){
		// How much data can be read from this steg piece
		int readLen = std::min(components[fileIndex]->getStegSize()-bytesToGo, length-bytesRead);
		status = components[fileIndex]->read(data+bytesRead, bytesToGo, readLen);
		if(status != SUCCESS){
			// Currently, if we get here, the filesystem is now probably corrupted
			std::cout << "Reading from " <<  components[fileIndex]->name << " failed.\n";
			return status;
		}
		fileIndex++;
		bytesToGo = 0;
		bytesRead += readLen;
	}

 	return SUCCESS;
}
