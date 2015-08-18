//////////////////////////////////////////////////////////////////////////////////////
// File     : xml_parser.c
// Date     : 14/04/2012
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
///////////////////////////////////////////////////////////////////////////////////////
// This program translate a "map.xml" source file to a binary file "map.bin" that 
// can be directly loaded in memory and used by the GIET-VM operating system.
//
// This map.xml file contains :
// 1) the multi-cluster/multi-processors hardware architecture description
// 2) the various multi-threaded software applications
// 3) the mapping directives bor both the tasks and the virtual segments.
// The corresponding C structures are defined in the "mapping_info.h" file.
///////////////////////////////////////////////////////////////////////////////////////

#include  <stdlib.h>
#include  <fcntl.h>
#include  <sys/types.h>
#include  <sys/stat.h>
#include  <unistd.h>
#include  <stdio.h>
#include  <string.h>
#include  <assert.h>
#include  <libxml/xmlreader.h>
#include  <mapping_info.h>
#include  <irq_handler.h>
#include  <giet_config.h>

#define MAX_CLUSTERS   1024
#define MAX_PSEGS      4096
#define MAX_VSPACES    1024
#define MAX_TASKS      4096
#define MAX_VSEGS      4096
#define MAX_PROCS      1024
#define MAX_IRQS       8192
#define MAX_PERIPHS    8192

#define XML_PARSER_DEBUG  1

///////////////////////////////////////////////////////////////////////////////////
//     global variables used to store and index the data structures
///////////////////////////////////////////////////////////////////////////////////

mapping_header_t *   header;
mapping_cluster_t *  cluster[MAX_CLUSTERS];  // cluster array
mapping_pseg_t *     pseg[MAX_PSEGS];        // pseg array
mapping_vspace_t *   vspace[MAX_VSPACES];    // vspace array
mapping_vseg_t *     vseg[MAX_VSEGS];        // vseg array
mapping_task_t *     task[MAX_TASKS];        // task array
mapping_proc_t *     proc[MAX_PROCS];        // proc array
mapping_irq_t *      irq[MAX_IRQS];          // irq array
mapping_periph_t *   periph[MAX_PERIPHS];    // peripheral array

// Index for the various arrays

unsigned int cluster_index  = 0;
unsigned int vspace_index = 0;
unsigned int global_index = 0;
unsigned int pseg_index = 0;        

unsigned int proc_index = 0;
unsigned int proc_loc_index = 0;

unsigned int irq_index = 0;
unsigned int irq_loc_index  = 0;

unsigned int periph_index = 0;
unsigned int periph_loc_index = 0;

unsigned int vseg_index = 0;
unsigned int vseg_loc_index = 0;

unsigned int task_index = 0;
unsigned int task_loc_index = 0;


//////////////////////////////////////////////////
unsigned int getIntValue( xmlTextReaderPtr reader, 
                          const char * attributeName, 
                          unsigned int * ok) 
{
    unsigned int value = 0;
    unsigned int i;
    char c;

    char * string = (char *) xmlTextReaderGetAttribute(reader, 
                                (const xmlChar *) attributeName);

    if (string == NULL) 
    {
        // missing argument
        *ok = 0;
        return 0;
    }
    else 
    {
        if ((string[0] == '0') && ((string[1] == 'x') || (string[1] == 'X'))) 
        {
            // Hexa
            for (i = 2 ; (string[i] != 0) && (i < 10) ; i++) 
            {
                c = string[i];
                if      ((c >= '0') && (c <= '9')) { value = (value << 4) + string[i] - 48; }
                else if ((c >= 'a') && (c <= 'f')) { value = (value << 4) + string[i] - 87; }
                else if ((c >= 'A') && (c <= 'F')) { value = (value << 4) + string[i] - 55; }
                else 
                {
                    *ok = 0;
                    return 0;
                }
            }
        }
        else 
        {
            // Decimal
            for (i = 0; (string[i] != 0) && (i < 9); i++) 
            {
                c = string[i];
                if ((c >= '0') && (c <= '9')) value = (value * 10) + string[i] - 48;
                else 
                {
                    *ok = 0;
                    return 0;
                }
            }
        }
        *ok = 1;
        return value; 
    }
} // end getIntValue()

////////////////////////////////////////////////
paddr_t getPaddrValue( xmlTextReaderPtr reader, 
                       const char * attributeName, 
                       unsigned int * ok) 
{
    paddr_t value = 0;
    unsigned int i;
    char c;

    char * string = (char *) xmlTextReaderGetAttribute(reader, 
                                (const xmlChar *) attributeName);

    if (string == NULL) 
    {
        // missing argument
        *ok = 0;
        return 0;
    }
    else 
    {
        if ((string[0] == '0') && ((string[1] == 'x') || (string[1] == 'X'))) 
        {
            // Hexa
            for (i = 2 ; (string[i] != 0) && (i < 18) ; i++) 
            {
                c = string[i];
                if      ((c >= '0') && (c <= '9')) { value = (value << 4) + string[i] - 48; }
                else if ((c >= 'a') && (c <= 'f')) { value = (value << 4) + string[i] - 87; }
                else if ((c >= 'A') && (c <= 'F')) { value = (value << 4) + string[i] - 55; }
                else 
                {
                    *ok = 0;
                    return 0;
                }
            }
        }
        else 
        {
            // Decimal not supported for paddr_t
            *ok = 0;
            return 0;
        }
        *ok = 1;
        return value; 
    }
} // end getPaddrValue()

////////////////////////////////////////////////
char * getStringValue( xmlTextReaderPtr reader, 
                       const char * attributeName, 
                       unsigned int * ok ) 
{
    char * string = (char *) xmlTextReaderGetAttribute(reader, 
                               (const xmlChar *) attributeName);


    if (string == NULL) 
    {
        // missing argument
        *ok = 0;
        return NULL;
    }
    else 
    {
        //we read only string smaller than 32 byte
        if (strlen(string) > 32) 
        {
            printf("[XML ERROR] all strings must be less than 32 bytes\n");
            exit(1);
        }

        *ok = 1;
        return string;
    }
} // end getStringValue()

///////////////////////////////////////////////////////////////
int getClusterId( unsigned int x, unsigned int y )
{
    // associative search of cluster index
    unsigned int cluster_id;

    for( cluster_id = 0 ; cluster_id < (header->x_size * header->y_size) ; cluster_id++ )
    {
        if( (cluster[cluster_id]->x == x) && (cluster[cluster_id]->y == y) )
        {
            return cluster_id;
        }
    }
    return -1;
}  // end getClusterId()

///////////////////////////////////////////////////////////////
int getPsegId(unsigned int x, unsigned int y, char * pseg_name) 
{
    int cluster_id = getClusterId( x, y );

    if ( cluster_id == -1 ) return -1;

    // associative search for pseg index
    unsigned int pseg_id;
    unsigned int pseg_min = cluster[cluster_id]->pseg_offset;
    unsigned int pseg_max = pseg_min + cluster[cluster_id]->psegs;

    for (pseg_id = pseg_min; pseg_id < pseg_max; pseg_id++) 
    {
        if (strcmp(pseg[pseg_id]->name, pseg_name) == 0) 
        {
            return pseg_id;
        }
    }
    return -1;
}  // end getPsegId()

///////////////////////////////////
int getVspaceId(char * vspace_name) 
{
    unsigned int vspace_id;

    for (vspace_id = 0; vspace_id < vspace_index; vspace_id++) 
    {
        if (strcmp(vspace[vspace_id]->name, vspace_name) == 0) 
        {
            return vspace_id;
        }
    }
    return -1;
}

///////////////////////////////////////////////////
int getVsegId( unsigned int vspace_id, char *name )
{
    unsigned int vseg_id;
    unsigned int vseg_min = vspace[vspace_id]->vseg_offset;
    unsigned int vseg_max = vseg_min + vspace[vspace_id]->vsegs;

    for (vseg_id = vseg_min ; vseg_id < vseg_max ; vseg_id++) 
    {
        if (strcmp(vseg[vseg_id]->name, name) == 0) return vseg_id;
    }
    return -1;
}


//////////////////////////////////////
void taskNode(xmlTextReaderPtr reader) 
{
    unsigned int ok;
    unsigned int value;
    unsigned int x,y;
    char * str;

    if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT) return;

    if (task_index >= MAX_TASKS) 
    {
        printf("[XML ERROR] The number of tasks is larger than %d\n", MAX_TASKS);
        exit(1);
    }

#if XML_PARSER_DEBUG
printf("   task %d\n", task_loc_index);
#endif

    task[task_index] = (mapping_task_t *) malloc(sizeof(mapping_task_t));

    ////////// get name attribute
    str = getStringValue(reader, "name", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
printf("      name      = %s\n", str);
#endif
        strncpy( task[task_index]->name, str, 31 );
    }
    else 
    {
        printf("[XML ERROR] illegal or missing <name> attribute for task (%d,%d)\n", 
                vspace_index, task_loc_index);
        exit(1);
    }

    ///////// get trdid attribute (optional)
    value = getIntValue(reader, "trdid", &ok);
#if XML_PARSER_DEBUG
printf("      trdid     = %d\n", value );
#endif
    if ( ok ) task[task_index]->trdid = value;
    else      task[task_index]->trdid = task_loc_index;

    ///////// get x coordinate 
    x = getIntValue(reader, "x", &ok);
#if XML_PARSER_DEBUG
printf("      x         = %d\n", x);
#endif
    if ( !(ok && (x < header->x_size)) ) 
    {
        printf("[XML ERROR] illegal or missing < x > attribute for task (%d,%d)\n",
                vspace_index, task_loc_index);
        exit(1);
    }  

    ///////// get y coordinate 
    y = getIntValue(reader, "y", &ok);
#if XML_PARSER_DEBUG
printf("      y         = %d\n", y);
#endif
    if ( !(ok && (y < header->y_size)) ) 
    {
        printf("[XML ERROR] illegal or missing < y > attribute for task (%d,%d)\n",
                vspace_index, task_loc_index);
        exit(1);
    }  

    ///////// set clusterid attribute
    int index = getClusterId( x, y );
#if XML_PARSER_DEBUG
printf("      clusterid = %d\n", index);
#endif
    if( index >= 0 )
    {
        task[task_index]->clusterid = index;
    }
    else
    {
        printf("[XML ERROR] <clusterid> not found for task (%d,%d)\n",
                vspace_index, task_loc_index);
        exit(1);
    }

    ////////// get p attribute
    value = getIntValue(reader, "p", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
printf("      proclocid = %x\n", value);
#endif
        if (value >= cluster[task[task_index]->clusterid]->procs) 
        {
            printf("[XML ERROR] <proclocid> too large for task (%d,%d)\n",
                    vspace_index, task_loc_index);
            exit(1);
        }
        task[task_index]->proclocid = value;
    }  
    else 
    {
        printf("[XML ERROR] illegal or missing <p> attribute for task (%d,%d)\n", 
                vspace_index, task_loc_index);
        exit(1);
    }

    ////////// get stackname attribute
    char* stack_name = getStringValue(reader, "stackname" , &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
printf("      stackname = %s\n", str);
#endif
        int index = getVsegId( vspace_index , stack_name );
        if (index >= 0) 
        {
#if XML_PARSER_DEBUG
printf("      stack_id  = %d\n", index);
#endif
            task[task_index]->stack_vseg_id = index;
        }
        else 
        {
            printf("[XML ERROR] illegal or missing <stackname> for task (%d,%d)\n", 
                    vspace_index, task_loc_index);
            exit(1);
        }
    }  
    else 
    {
        printf("[XML ERROR] illegal or missing <stackname> for task (%d,%d)\n", 
                vspace_index, task_loc_index);
        exit(1);
    }

    ////////// get heap attribute
    char* heap_name = getStringValue(reader, "heapname", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
printf("      heapname  = %s\n", str);
#endif
        int index = getVsegId( vspace_index , heap_name );
        if (index >= 0) 
        {
#if XML_PARSER_DEBUG
printf("      heap_id   = %d\n", index );
#endif
            task[task_index]->heap_vseg_id = index;
        }
        else 
        {
            printf("[XML ERROR] illegal or missing <heapname> for task (%d,%d)\n", 
                   vspace_index, task_loc_index);
            exit(1);
        }
    }  
    else 
    {
        task[task_index]->heap_vseg_id = -1;
    }

    ////////// get startid  attribute
    value = getIntValue(reader, "startid", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
printf("      startid   = %x\n", value);
#endif
        task[task_index]->startid = value;
    }  
    else 
    {
        printf("[XML ERROR] illegal or missing <startid> attribute for task (%d,%d)\n", 
                vspace_index, task_loc_index);
        exit(1);
    }

    task_index++;
    task_loc_index++;
} // end taskNode()


//////////////////////////////////////
void vsegNode(xmlTextReaderPtr reader) 
{
    unsigned int ok;
    unsigned int value;
    unsigned int x,y;
    char * str;

    if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT) return;

    if (vseg_index >= MAX_VSEGS) 
    {
        printf("[XML ERROR] The number of vsegs is larger than %d\n", MAX_VSEGS);
        exit(1);
    }

#if XML_PARSER_DEBUG
printf("    vseg %d\n", vseg_loc_index);
#endif

    vseg[vseg_index] = (mapping_vseg_t *) malloc(sizeof(mapping_vseg_t));

    ///////// set mapped attribute
    vseg[vseg_index]->mapped = 0;

    ///////// get name attribute
    str = getStringValue(reader, "name", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
printf("      name        = %s\n", str);
#endif
        strncpy( vseg[vseg_index]->name, str, 31);
    }
    else 
    {
        printf("[XML ERROR] illegal or missing <name> attribute for vseg (%d,%d)\n", 
                vspace_index, vseg_loc_index);
        exit(1);
    }

    ////////// get ident attribute (optional : 0 if missing)
    value = getIntValue(reader, "ident", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
printf("      ident       = %d\n", value);
#endif
        vseg[vseg_index]->ident = value;
    }  
    else 
    {
        vseg[vseg_index]->ident = 0;
    }

    ////////// get local attribute (optional : 0 if missing)
    value = getIntValue(reader, "local", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
printf("      local       = %d\n", value);
#endif
        vseg[vseg_index]->local = value;
    }  
    else 
    {
        vseg[vseg_index]->local = 0;
    }

    ////////// get big attribute (optional : 0 if missing)
    value = getIntValue(reader, "big", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
printf("      big         = %d\n", value);
#endif
        vseg[vseg_index]->big = value;
    }  
    else 
    {
        vseg[vseg_index]->big = 0;
    }

    /////////// get vbase attribute
    value = getIntValue(reader, "vbase", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
printf("      vbase       = 0x%x\n", value);
#endif
        vseg[vseg_index]->vbase = value;
    }
    else 
    {
        printf("[XML ERROR] illegal or missing <vbase> attribute for vseg (%d,%d)\n", 
                vspace_index, vseg_loc_index);
        exit(1);
    }

    ////////// get length attribute 
    value = getIntValue(reader, "length", &ok);
    if (ok)
    {
#if XML_PARSER_DEBUG
printf("      length      = %x\n", value);
#endif
        vseg[vseg_index]->length = value;
    }
    else 
    {
        printf("[XML ERROR] illegal or missing <length> attribute for vseg (%d,%d)\n",
                vspace_index, vseg_loc_index);
        exit(1);
    }

    //////// get type attribute
    str = getStringValue(reader, "type", &ok);

#if XML_PARSER_DEBUG
printf("      type        = %s\n", str);
#endif

    if      (ok && (strcmp(str, "ELF")    == 0)) vseg[vseg_index]->type = VSEG_TYPE_ELF;
    else if (ok && (strcmp(str, "PERI")   == 0)) vseg[vseg_index]->type = VSEG_TYPE_PERI;
    else if (ok && (strcmp(str, "BLOB")   == 0)) vseg[vseg_index]->type = VSEG_TYPE_BLOB;
    else if (ok && (strcmp(str, "PTAB")   == 0)) vseg[vseg_index]->type = VSEG_TYPE_PTAB;
    else if (ok && (strcmp(str, "BUFFER") == 0)) vseg[vseg_index]->type = VSEG_TYPE_BUFFER;
    else if (ok && (strcmp(str, "SCHED")  == 0)) vseg[vseg_index]->type = VSEG_TYPE_SCHED;
    else if (ok && (strcmp(str, "HEAP")   == 0)) vseg[vseg_index]->type = VSEG_TYPE_HEAP;
    else
    {
        printf("[XML ERROR] illegal or missing <type> attribute for vseg (%d,%d)\n",
                vspace_index, vseg_loc_index);
        exit(1);
    }

    ////////// get x coordinate
    x = getIntValue(reader, "x", &ok);
#if XML_PARSER_DEBUG
printf("      x           = %d\n", x);
#endif
    if ( !(ok && (x < header->x_size)) ) 
    {
        printf("[XML ERROR] illegal or missing < x > attribute for vseg %d\n", 
                vseg_loc_index);
        exit(1);
    }

    ////////// get y coordinate
    y = getIntValue(reader, "y", &ok);
#if XML_PARSER_DEBUG
printf("      y           = %d\n", y);
#endif
    if ( !(ok && (y < header->y_size)) ) 
    {
        printf("[XML ERROR] illegal or missing < y > attribute for vseg %d\n", 
                vseg_loc_index);
        exit(1);
    }

    ///////// get psegname attribute
    str = getStringValue(reader, "psegname", &ok);
#if XML_PARSER_DEBUG
printf("      psegname    = %s\n", str);
#endif
    if (ok == 0) 
    {
        printf("[XML ERROR] illegal or missing <psegname> for vseg %d\n", 
                vseg_loc_index);
        exit(1);
    }

    /////////// set psegid field
    int psegid = getPsegId( x, y, str );
#if XML_PARSER_DEBUG
printf("      psegid      = %d\n", psegid);
#endif
    if (psegid >= 0) 
    {
        vseg[vseg_index]->psegid = psegid;
    }
    else 
    {
        printf("[XML ERROR] pseg not found for vseg %d / x = %d / y = %d / psegname = %s\n", 
                vseg_loc_index, x, y, str );
        exit(1);
    }  

    //////// get mode attribute
    str = getStringValue(reader, "mode", &ok);
#if XML_PARSER_DEBUG
printf("      mode        = %s\n", str);
#endif
    if      (ok && (strcmp(str, "CXWU") == 0)) { vseg[vseg_index]->mode = 0xF; }
    else if (ok && (strcmp(str, "CXW_") == 0)) { vseg[vseg_index]->mode = 0xE; }
    else if (ok && (strcmp(str, "CX_U") == 0)) { vseg[vseg_index]->mode = 0xD; }
    else if (ok && (strcmp(str, "CX__") == 0)) { vseg[vseg_index]->mode = 0xC; }
    else if (ok && (strcmp(str, "C_WU") == 0)) { vseg[vseg_index]->mode = 0xB; }
    else if (ok && (strcmp(str, "C_W_") == 0)) { vseg[vseg_index]->mode = 0xA; }
    else if (ok && (strcmp(str, "C__U") == 0)) { vseg[vseg_index]->mode = 0x9; }
    else if (ok && (strcmp(str, "C___") == 0)) { vseg[vseg_index]->mode = 0x8; }
    else if (ok && (strcmp(str, "_XWU") == 0)) { vseg[vseg_index]->mode = 0x7; }
    else if (ok && (strcmp(str, "_XW_") == 0)) { vseg[vseg_index]->mode = 0x6; }
    else if (ok && (strcmp(str, "_X_U") == 0)) { vseg[vseg_index]->mode = 0x5; }
    else if (ok && (strcmp(str, "_X__") == 0)) { vseg[vseg_index]->mode = 0x4; }
    else if (ok && (strcmp(str, "__WU") == 0)) { vseg[vseg_index]->mode = 0x3; }
    else if (ok && (strcmp(str, "__W_") == 0)) { vseg[vseg_index]->mode = 0x2; }
    else if (ok && (strcmp(str, "___U") == 0)) { vseg[vseg_index]->mode = 0x1; }
    else if (ok && (strcmp(str, "____") == 0)) { vseg[vseg_index]->mode = 0x0; }
    else {
        printf("[XML ERROR] illegal or missing <mode> attribute for vseg (%d,%d)\n", 
                vspace_index, vseg_loc_index);
        exit(1);
    }

    ////////// get binpath attribute (optional : "" if missing)
    str = getStringValue(reader, "binpath", &ok);
    if (ok)
    {
#if XML_PARSER_DEBUG
printf("      binpath = %s\n", str);
#endif
        strncpy(vseg[vseg_index]->binpath, str, 63);
    }
    else
    {
        vseg[vseg_index]->binpath[0] = 0;
    }

    vseg_index++;
    vseg_loc_index++;
} // end vsegNode()

////////////////////////////////////////
void vspaceNode(xmlTextReaderPtr reader) 
{
    unsigned int ok;

    vseg_loc_index = 0;
    task_loc_index = 0;

    if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT) return;

    vspace[vspace_index] = (mapping_vspace_t *) malloc(sizeof(mapping_vspace_t));
    header->vspaces      = header->vspaces + 1;

    ////////// get name attribute
    char* vspace_name = getStringValue(reader, "name", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
printf("\n  vspace = %s\n", vspace_name );
#endif
        strncpy( vspace[vspace_index]->name, vspace_name , 31 );
    }
    else 
    {
        printf("[XML ERROR] illegal or missing <name> attribute for vspace %d\n", 
                vspace_index);
        exit(1);
    }

    ////////// get active attribute (optional / default is 0)
    unsigned int value = getIntValue(reader, "active", &ok);
    if (ok) vspace[vspace_index]->active = value;
    else    vspace[vspace_index]->active = 0;
    
    ////////// set vseg_offset and task_offset attributes
    vspace[vspace_index]->vseg_offset = vseg_index;
    vspace[vspace_index]->task_offset = task_index;

    ////////// initialise vsegs and tasks attributes
    vspace[vspace_index]->vsegs = 0;
    vspace[vspace_index]->tasks = 0;

    ////////// get startname attribute
    char* start_name = getStringValue(reader, "startname", &ok);
    if (ok == 0) 
    {
        printf("[XML ERROR] illegal or missing <startname> attribute for vspace %s\n", 
                vspace[vspace_index]->name);
        exit(1);
    }

    int status = xmlTextReaderRead(reader);
    while (status == 1) 
    {
        const char * tag = (const char *) xmlTextReaderConstName(reader);

        if (strcmp(tag, "vseg") == 0) 
        {
            vsegNode(reader);
            vspace[vspace_index]->vsegs += 1;
        }
        else if (strcmp(tag, "task") == 0) 
        {
            taskNode(reader);
            vspace[vspace_index]->tasks += 1;
        }
        else if (strcmp(tag, "#text")    == 0) { }
        else if (strcmp(tag, "#comment") == 0) { }
        else if (strcmp(tag, "vspace")   == 0) 
        {
            // get index of the vseg containing the start vector
            int index = getVsegId( vspace_index, start_name );
            if (index == -1) 
            {
                printf("[XML ERROR] vseg containing start vector not found in vspace %s\n",
                        vspace[vspace_index]->name);
                exit(1);
            }
            else 
            {
                vspace[vspace_index]->start_vseg_id = index;
            }

#if XML_PARSER_DEBUG
printf("      vsegs       = %d\n", vspace[vspace_index]->vsegs );
printf("      tasks       = %d\n", vspace[vspace_index]->tasks );
printf("      vseg_offset = %d\n", vspace[vspace_index]->vseg_offset );
printf("      task_offset = %d\n", vspace[vspace_index]->task_offset );
printf("      start_id    = %d\n", vspace[vspace_index]->start_vseg_id );
printf("      active      = %x\n", vspace[vspace_index]->active );
printf("  end vspace %d\n\n", vspace_index);
#endif
            vspace_index++;
            return;
        }
        else 
        {
            printf("[XML ERROR] Unknown tag %s", tag);
            exit(1);
        }
        status = xmlTextReaderRead(reader);
    }
} // end vspaceNode()

/////////////////////////////////////
void irqNode(xmlTextReaderPtr reader) 
{
    unsigned int ok;
    unsigned int value;
    char * str;

    if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT) return;

    if (irq_index >= MAX_IRQS) 
    {
        printf("[XML ERROR] The number of irqs is larger than %d\n", MAX_IRQS);
    }

#if XML_PARSER_DEBUG
printf("      irq %d\n", irq_loc_index);
#endif

    irq[irq_index] = (mapping_irq_t *) malloc(sizeof(mapping_irq_t));

    ///////// get srcid attribute
    value = getIntValue(reader, "srcid", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
printf("        srcid   = %d\n", value);
#endif
        irq[irq_index]->srcid = value;
        if (value >= 32) 
        {
            printf("[XML ERROR] IRQ <srcid> too large for periph %d in cluster %d\n",
                    cluster_index, periph_loc_index);
            exit(1);
        }
    }
    else 
    {
        printf("[XML ERROR] missing IRQ <srcid> for periph %d in cluster %d\n",
                cluster_index, periph_loc_index);
        exit(1);
    }

    ///////// get srctype attribute 
    str = getStringValue(reader, "srctype", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
printf("        srctype = %s\n", str);
#endif
        if      ( strcmp(str, "HWI") == 0 ) irq[irq_index]->srctype = IRQ_TYPE_HWI;
        else if ( strcmp(str, "WTI") == 0 ) irq[irq_index]->srctype = IRQ_TYPE_WTI;
        else if ( strcmp(str, "PTI") == 0 ) irq[irq_index]->srctype = IRQ_TYPE_PTI;
        else    
        {
            printf("[XML ERROR] illegal IRQ <srctype> for periph %d in cluster %d\n",
                   cluster_index, periph_loc_index);
            exit(1);
        }
    }
    else 
    {
        printf("[XML ERROR] missing IRQ <srctype> for periph %d in cluster %d\n",
               cluster_index, periph_loc_index);
        exit(1);
    }

    ///////// get isr attribute 
    str = getStringValue(reader, "isr", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
printf("        isr     = %s\n", str);
#endif
        if      (strcmp(str, "ISR_DEFAULT") == 0)  irq[irq_index]->isr = ISR_DEFAULT;
        else if (strcmp(str, "ISR_TICK"   ) == 0)  irq[irq_index]->isr = ISR_TICK;
        else if (strcmp(str, "ISR_TTY_RX" ) == 0)  irq[irq_index]->isr = ISR_TTY_RX;
        else if (strcmp(str, "ISR_TTY_TX" ) == 0)  irq[irq_index]->isr = ISR_TTY_TX;
        else if (strcmp(str, "ISR_BDV"    ) == 0)  irq[irq_index]->isr = ISR_BDV;
        else if (strcmp(str, "ISR_TIMER"  ) == 0)  irq[irq_index]->isr = ISR_TIMER;
        else if (strcmp(str, "ISR_WAKUP"  ) == 0)  irq[irq_index]->isr = ISR_WAKUP;
        else if (strcmp(str, "ISR_NIC_RX" ) == 0)  irq[irq_index]->isr = ISR_NIC_RX;
        else if (strcmp(str, "ISR_NIC_TX" ) == 0)  irq[irq_index]->isr = ISR_NIC_TX;
        else if (strcmp(str, "ISR_CMA"    ) == 0)  irq[irq_index]->isr = ISR_CMA;
        else if (strcmp(str, "ISR_MMC"    ) == 0)  irq[irq_index]->isr = ISR_MMC;
        else if (strcmp(str, "ISR_DMA"    ) == 0)  irq[irq_index]->isr = ISR_DMA;
        else if (strcmp(str, "ISR_SDC"    ) == 0)  irq[irq_index]->isr = ISR_SDC;
        else if (strcmp(str, "ISR_MWR"    ) == 0)  irq[irq_index]->isr = ISR_MWR;
        else if (strcmp(str, "ISR_HBA"    ) == 0)  irq[irq_index]->isr = ISR_HBA;
        else 
        {
            printf("[XML ERROR] illegal IRQ <isr> for periph %d in cluster %d\n",
                   periph_loc_index, cluster_index );
            exit(1);
        }
    }  
    else 
    {
        printf("[XML ERROR] missing IRQ <isr> for periph %d in cluster %d\n",
                cluster_index, periph_loc_index);
        exit(1);
    }

    ///////// get channel attribute (optionnal : 0 if missing)
    value = getIntValue(reader, "channel", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
printf("        channel = %d\n", value);
#endif
        irq[irq_index]->channel = value;
    }
    else 
    {
        irq[irq_index]->channel = 0;
    }

    irq_index++;
    irq_loc_index++;

} // end irqNode


////////////////////////////////////////
void periphNode(xmlTextReaderPtr reader) 
{
    char * str;
    unsigned int value;
    unsigned int ok;

    irq_loc_index = 0;

    if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT) return;

    if (periph_index >= MAX_PERIPHS) 
    {
        printf("[XML ERROR] The number of periphs is larger than %d\n", MAX_PERIPHS);
        exit(1);
    }

#if XML_PARSER_DEBUG
printf("\n    periph %d\n", periph_index);
#endif

    periph[periph_index] = (mapping_periph_t *) malloc(sizeof(mapping_periph_t));

    ///////// get channels attribute (optionnal : 1 if missing)
    value = getIntValue(reader, "channels", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
printf("      channels    = %d\n", value);
#endif
        periph[periph_index]->channels = value;
    }
    else 
    {
        periph[periph_index]->channels = 1;
    }

    ///////// get arg0 attribute (optionnal : undefined if missing)
    value = getIntValue(reader, "arg0", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
printf("      arg0        = %d\n", value);
#endif
        periph[periph_index]->arg0 = value;
    }

    ///////// get arg1 attribute (optionnal : undefined if missing)
    value = getIntValue(reader, "arg1", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
printf("      arg1        = %d\n", value);
#endif
        periph[periph_index]->arg1 = value;
    }

    ///////// get arg2 attribute (optionnal : undefined if missing)
    value = getIntValue(reader, "arg2", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
printf("      arg2        = %d\n", value);
#endif
        periph[periph_index]->arg2 = value;
    }

    ///////// get arg3 attribute (optionnal : undefined if missing)
    value = getIntValue(reader, "arg3", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
printf("      arg3        = %d\n", value);
#endif
        periph[periph_index]->arg3 = value;
    }

    /////////// get psegname attribute 
    str = getStringValue(reader, "psegname", &ok);
    if (ok == 0) 
    {
        printf("[XML ERROR] illegal or missing <psegname> for periph %d in cluster %d\n", 
                periph_index, cluster_index);
        exit(1);
    }

    /////////// set psegid attribute
    int index = getPsegId( cluster[cluster_index]->x, cluster[cluster_index]->y, str);
    if (index >= 0) 
    {
#if XML_PARSER_DEBUG
printf("      clusterid   = %d\n", cluster_index);
printf("      psegname    = %s\n", str);
printf("      psegid      = %d\n", index);
#endif
        periph[periph_index]->psegid = index;
    }
    else 
    {
        printf("[XML ERROR] pseg not found for periph %d / clusterid = %d / psegname = %s\n", 
                periph_loc_index, cluster_index, str );
        exit(1);
    }  

    /////////// get type attribute 
    str = getStringValue(reader, "type", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
printf("      type        = %s\n", str);
#endif
        if      (strcmp(str, "CMA" ) == 0) periph[periph_index]->type = PERIPH_TYPE_CMA;
        else if (strcmp(str, "DMA" ) == 0) periph[periph_index]->type = PERIPH_TYPE_DMA;
        else if (strcmp(str, "FBF" ) == 0) periph[periph_index]->type = PERIPH_TYPE_FBF;
        else if (strcmp(str, "IOB" ) == 0) periph[periph_index]->type = PERIPH_TYPE_IOB;
        else if (strcmp(str, "IOC" ) == 0) periph[periph_index]->type = PERIPH_TYPE_IOC;
        else if (strcmp(str, "MMC" ) == 0) periph[periph_index]->type = PERIPH_TYPE_MMC;
        else if (strcmp(str, "MWR" ) == 0) periph[periph_index]->type = PERIPH_TYPE_MWR;
        else if (strcmp(str, "NIC" ) == 0) periph[periph_index]->type = PERIPH_TYPE_NIC;
        else if (strcmp(str, "ROM" ) == 0) periph[periph_index]->type = PERIPH_TYPE_ROM;
        else if (strcmp(str, "SIM" ) == 0) periph[periph_index]->type = PERIPH_TYPE_SIM;
        else if (strcmp(str, "SIM" ) == 0) periph[periph_index]->type = PERIPH_TYPE_TIM;
        else if (strcmp(str, "TTY" ) == 0) periph[periph_index]->type = PERIPH_TYPE_TTY;
        else if (strcmp(str, "XCU" ) == 0) periph[periph_index]->type = PERIPH_TYPE_XCU;
        else if (strcmp(str, "PIC" ) == 0) periph[periph_index]->type = PERIPH_TYPE_PIC;
        else if (strcmp(str, "DROM") == 0) periph[periph_index]->type = PERIPH_TYPE_DROM;
        else
        {
            printf("[XML ERROR] illegal peripheral type: %s in cluster %d\n",
                    str, cluster_index);
            exit(1);
        }
    }

    ////////// get subtype if IOC
    if (periph[periph_index]->type == PERIPH_TYPE_IOC )
    {
        char* subtype = getStringValue(reader, "subtype", &ok);
        if (ok)
        {
#if XML_PARSER_DEBUG
printf("      subtype     = %s\n", str);
#endif
            if      (strcmp(subtype, "BDV") == 0) 
            periph[periph_index]->subtype = IOC_SUBTYPE_BDV;
            else if (strcmp(subtype, "HBA") == 0) 
            periph[periph_index]->subtype = IOC_SUBTYPE_HBA;
            else if (strcmp(subtype, "SDC") == 0) 
            periph[periph_index]->subtype = IOC_SUBTYPE_SDC;
            else if (strcmp(subtype, "SPI") == 0) 
            periph[periph_index]->subtype = IOC_SUBTYPE_SPI;
        }
        else
        {
            printf("[XML ERROR] illegal subtype for IOC peripheral\n");
            exit(1);
        }
    }
    else
    {
        periph[periph_index]->subtype = 0XFFFFFFFF;
    }

    ////////// get subtype if MWR
    if (periph[periph_index]->type == PERIPH_TYPE_MWR )
    {
        char* subtype = getStringValue(reader, "subtype", &ok);
        if (ok)
        {
#if XML_PARSER_DEBUG
printf("      subtype     = %s\n", str);
#endif
            if      (strcmp(subtype, "GCD") == 0) 
            periph[periph_index]->subtype = MWR_SUBTYPE_GCD;
            else if (strcmp(subtype, "DCT") == 0) 
            periph[periph_index]->subtype = MWR_SUBTYPE_DCT;
            else if (strcmp(subtype, "CPY") == 0) 
            periph[periph_index]->subtype = MWR_SUBTYPE_CPY;
        }
        else
        {
            printf("[XML ERROR] illegal subtype for MWR peripheral\n");
            exit(1);
        }
    }
    else
    {
        periph[periph_index]->subtype = 0XFFFFFFFF;
    }

    ////////////// set irq_offset attribute
    periph[periph_index]->irq_offset = irq_index;

    ///////////// get IRQs
    int status = xmlTextReaderRead(reader);
    while (status == 1) 
    {
        const char * tag = (const char *) xmlTextReaderConstName(reader);

        if (strcmp(tag, "irq") == 0) 
        {
            if ( (periph[periph_index]->type != PERIPH_TYPE_XCU) &&
                 (periph[periph_index]->type != PERIPH_TYPE_PIC) )
            {
                printf("[XML ERROR] periph %d in cluster(%d,%d) "
                       " only XCU and PIC can contain IRQs",
                periph_loc_index, cluster[cluster_index]->x, cluster[cluster_index]->y);
                exit(1);
            }
            else
            {
                  irqNode(reader);
            }
        }
        else if (strcmp(tag, "#text")    == 0) { }
        else if (strcmp(tag, "#comment") == 0) { }
        else if (strcmp(tag, "periph")   == 0) 
        {
            periph[periph_index]->irqs = irq_loc_index;
            cluster[cluster_index]->periphs++;
            periph_loc_index++;
            periph_index++;

#if XML_PARSER_DEBUG
printf("      irqs        = %d\n", irq_loc_index);
printf("      irq_offset  = %d\n", irq_index);
#endif
            return;
        }
        else 
        {
            printf("[XML ERROR] Unknown tag %s", tag);
            exit(1);
        }
        status = xmlTextReaderRead(reader);
    }
} // end periphNode

//////////////////////////////////////
void procNode(xmlTextReaderPtr reader) 
{
    unsigned int ok;
    unsigned int value;

    if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT) return;

    if (proc_index >= MAX_PROCS) 
    {
        printf("[XML ERROR] The number of procs is larger than %d\n", MAX_PROCS);
        exit(1);
    }

#if XML_PARSER_DEBUG
printf("\n    proc %d\n", proc_index);
#endif

    proc[proc_index] = (mapping_proc_t *) malloc(sizeof(mapping_proc_t));

    /////////// get index attribute (optional)
    value = getIntValue(reader, "index", &ok);
    if (ok && (value != proc_loc_index)) 
    {
        printf("[XML ERROR] wrong local proc index / expected value is %d", 
                proc_loc_index);
        exit(1);
    }
    proc[proc_index]->index = proc_loc_index;

    cluster[cluster_index]->procs++;
    proc_loc_index++;
    proc_index++;
} // end procNode()


//////////////////////////////////////
void psegNode(xmlTextReaderPtr reader) 
{
    unsigned int ok;
    paddr_t      ll_value;
    char * str;

    if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT) return;

    if (pseg_index >= MAX_PSEGS) 
    {
        printf("[XML ERROR] The number of psegs is larger than %d\n", MAX_PSEGS);
        exit(1);
    }

#if XML_PARSER_DEBUG
printf("    pseg %d\n", pseg_index);
#endif

    pseg[pseg_index] = (mapping_pseg_t *) malloc(sizeof(mapping_pseg_t));

    /////// get name attribute
    str = getStringValue(reader, "name", &ok);
#if XML_PARSER_DEBUG
printf("      name = %s\n", str);
#endif
    if (ok) 
    {
        strncpy(pseg[pseg_index]->name, str, 31);
    }
    else 
    {
        printf("[XML ERROR] illegal or missing <name> for pseg %d in cluster %d\n",
                pseg_index, cluster_index);
        exit(1);
    }

    //////// get type attribute
    str = getStringValue(reader, "type", &ok);
#if XML_PARSER_DEBUG
printf("      type = %s\n", str);
#endif
    if      (ok && (strcmp(str, "RAM" ) == 0)) { pseg[pseg_index]->type = PSEG_TYPE_RAM; }
    else if (ok && (strcmp(str, "PERI") == 0)) { pseg[pseg_index]->type = PSEG_TYPE_PERI; }
    else 
    {
        printf("[XML ERROR] illegal or missing <type> for pseg %s in cluster %d\n",
                pseg[pseg_index]->name, cluster_index);
        exit(1);
    }

    //////// get base attribute
    ll_value = getPaddrValue(reader, "base", &ok);
#if XML_PARSER_DEBUG
printf("      base = 0x%llx\n", ll_value);
#endif
    if (ok) 
    {
        pseg[pseg_index]->base = ll_value;
    }
    else {
        printf("[XML ERROR] illegal or missing <base> for pseg %s in cluster %d\n",
                pseg[pseg_index]->name, cluster_index);
        exit(1);
    }

    //////// get length attribute
    ll_value = getPaddrValue(reader, "length", &ok);
#if XML_PARSER_DEBUG
printf("      length = 0x%llx\n", ll_value);
#endif
    if (ok) 
    {
        pseg[pseg_index]->length = ll_value;
    }  
    else 
    {
        printf("[XML ERROR] illegal or missing <length> for pseg %s in cluster %d\n",
                pseg[pseg_index]->name, cluster_index);
        exit(1);
    }

    //////// set cluster attribute
    pseg[pseg_index]->clusterid = cluster_index;

    //////// set next_vseg attribute
    pseg[pseg_index]->next_vseg = 0;

    pseg_index++;
    cluster[cluster_index]->psegs++;
} // end psegNode()


/////////////////////////////////////////
void clusterNode(xmlTextReaderPtr reader) 
{
    unsigned int ok;
    unsigned int value;

    cluster[cluster_index] = (mapping_cluster_t *) malloc(sizeof(mapping_cluster_t));

    //initialise variables that will be incremented by *Node() functions
    cluster[cluster_index]->psegs = 0;
    cluster[cluster_index]->procs = 0;
    cluster[cluster_index]->periphs = 0;

    //initialise global variables
    proc_loc_index = 0;
    periph_loc_index = 0;

    if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT)  return;

#if XML_PARSER_DEBUG
printf("\n  cluster %d\n", cluster_index);
#endif

    /////////// get x coordinate
    value = getIntValue(reader, "x", &ok);
#if XML_PARSER_DEBUG
printf("    x             = %d\n", value);
#endif
    if (ok && (value < header->x_size) ) 
    {
        cluster[cluster_index]->x = value;
    }
    else
    {
        printf("[XML ERROR] Illegal or missing < x > attribute for cluster %d", 
                cluster_index);
        exit(1);
    }

    /////////// get y coordinate
    value = getIntValue(reader, "y", &ok);
#if XML_PARSER_DEBUG
printf("    y             = %d\n", value);
#endif
    if (ok && (value < header->y_size) ) 
    {
        cluster[cluster_index]->y = value;
    }
    else
    {
        printf("[XML ERROR] Illegal or missing < y > attribute for cluster %d", 
                cluster_index);
        exit(1);
    }

    ////////// set offsets
    cluster[cluster_index]->pseg_offset = pseg_index;
    cluster[cluster_index]->proc_offset = proc_index;
    cluster[cluster_index]->periph_offset = periph_index;

#if XML_PARSER_DEBUG
printf("    pseg_offset   = %d\n", pseg_index);
printf("    proc_offset   = %d\n", proc_index);
printf("    periph_offset = %d\n", periph_index);
#endif

    ////////// get psegs, procs, and periphs
    int status = xmlTextReaderRead(reader);

    while (status == 1) 
    {
        const char * tag = (const char *) xmlTextReaderConstName(reader);

        if      (strcmp(tag, "pseg")     == 0) psegNode(reader);
        else if (strcmp(tag, "proc")     == 0) procNode(reader);
        else if (strcmp(tag, "periph")   == 0) periphNode(reader);
        else if (strcmp(tag, "#text")    == 0) { }
        else if (strcmp(tag, "#comment") == 0) { }
        else if (strcmp(tag, "cluster")  == 0) 
        {

#if XML_PARSER_DEBUG
printf("    psegs   = %d\n", cluster[cluster_index]->psegs);
printf("    procs   = %d\n", cluster[cluster_index]->procs);
printf("    periphs = %d\n", cluster[cluster_index]->periphs);
printf("    end cluster %d\n", cluster_index);
#endif
            cluster_index++;
            return;
        }
        status = xmlTextReaderRead(reader);
    }
} // end clusterNode()


//////////////////////////////////////////////
void clusterSetNode(xmlTextReaderPtr reader) 
{
    if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT) return;

#if XML_PARSER_DEBUG
printf("\n  clusters set\n");
#endif

    int status = xmlTextReaderRead(reader);
    while (status == 1) 
    {
        const char * tag = (const char *) xmlTextReaderConstName(reader);

        if      (strcmp(tag, "cluster")    == 0) { clusterNode(reader); }
        else if (strcmp(tag, "#text")      == 0) { }
        else if (strcmp(tag, "#comment")   == 0) { }
        else if (strcmp(tag, "clusterset") == 0) 
        {
            // checking number of clusters
            if ( cluster_index != (header->x_size * header->y_size) ) 
            {
                printf("[XML ERROR] Wrong number of clusters\n");
                exit(1);
            }

#if XML_PARSER_DEBUG
            printf("  end cluster set\n\n");
#endif
            header->psegs   = pseg_index;
            header->procs   = proc_index;
            header->irqs    = irq_index;
            header->periphs = periph_index;
            return;
        }
        else 
        {
            printf("[XML ERROR] Unknown tag in clusterset node : %s",tag);
            exit(1);
        }
        status = xmlTextReaderRead(reader);
    }
} // end clusterSetNode()


///////////////////////////////////////////
void globalSetNode(xmlTextReaderPtr reader) 
{
    if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT) return;

#if XML_PARSER_DEBUG
    printf("  globals set\n");
#endif

    int status = xmlTextReaderRead(reader);
    while (status == 1) 
    {
        const char * tag = (const char *) xmlTextReaderConstName(reader);

        if      (strcmp(tag, "vseg")      == 0) 
        { 
            vsegNode( reader ); 
            header->globals = header->globals + 1;
        }
        else if (strcmp(tag, "#text")     == 0) { }
        else if (strcmp(tag, "#comment")  == 0) { }
        else if (strcmp(tag, "globalset") == 0) 
        {
#if XML_PARSER_DEBUG
            printf("  end global set\n\n");
#endif
            vseg_loc_index = 0;
            return;
        }
        else 
        {
            printf("[XML ERROR] Unknown tag in globalset node : %s",tag);
            exit(1);
        }
        status = xmlTextReaderRead(reader);
    }
} // end globalSetNode()


///////////////////////////////////////////
void vspaceSetNode(xmlTextReaderPtr reader)
{
    if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT) {
        return;
    }

#if XML_PARSER_DEBUG
    printf("\n  vspaces set\n");
#endif

    int status = xmlTextReaderRead ( reader );
    while (status == 1) {
        const char * tag = (const char *) xmlTextReaderConstName(reader);

        if (strcmp(tag, "vspace") == 0) {
            vspaceNode(reader);
        }
        else if (strcmp(tag, "#text"    ) == 0 ) { }
        else if (strcmp(tag, "#comment" ) == 0 ) { }
        else if (strcmp(tag, "vspaceset") == 0 ) 
        {
            header->vsegs = vseg_index;
            header->tasks = task_index;
            return;
        }
        else 
        {
            printf("[XML ERROR] Unknown tag in vspaceset node : %s",tag);
            exit(1);
        }
        status = xmlTextReaderRead(reader);
    }
} // end globalSetNode()


////////////////////////////////////////
void headerNode(xmlTextReaderPtr reader) 
{
    char * name;
    unsigned int value;
    unsigned int ok;

    if (xmlTextReaderNodeType(reader) == XML_READER_TYPE_END_ELEMENT) return;

#if XML_PARSER_DEBUG
    printf("mapping_info\n");
#endif

    header = (mapping_header_t *) malloc(sizeof(mapping_header_t));

    ////////// get name attribute
    name = getStringValue(reader, "name", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
        printf("  name = %s\n", name);
#endif
        strncpy( header->name, name, 31);
    }
    else 
    {
        printf("[XML ERROR] illegal or missing <name> attribute in header\n");
        exit(1);
    }

    /////////// get signature attribute
    value = getIntValue(reader, "signature", &ok);
    if ( ok && (value == IN_MAPPING_SIGNATURE) )
    {
#if XML_PARSER_DEBUG
        printf("  signature = %x\n", value);
#endif
        header->signature = IN_MAPPING_SIGNATURE;
    }
    else
    {
        printf("[XML ERROR] illegal or missing <signature> for mapping %s\n",
        header->name );
        exit(1);
    }

    /////////// get x_width attribute
    value = getIntValue(reader, "x_width", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
        printf("  x_width = %d\n", value);
#endif
        header->x_width = value;
    }

    /////////// get y_width attribute
    value = getIntValue(reader, "y_width", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
        printf("  y_width = %d\n", value);
#endif
        header->y_width = value;
    }

    /////////// get x_size attribute
    unsigned int x_size = getIntValue(reader, "x_size", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
        printf("  x_size  = %d\n", x_size);
#endif
        header->x_size = x_size;
    }
    else 
    {
        printf("[XML ERROR] illegal or missing <x_size> attribute in header\n");
        exit(1);
    }

    /////////// get y_size attribute
    unsigned int y_size = getIntValue(reader, "y_size", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
        printf("  y_size  = %d\n", y_size);
#endif
        header->y_size = y_size;
    }
    else 
    {
        printf("[XML ERROR] illegal or missing <y_size> attribute in header\n");
        exit(1);
    }

    /////////// get x_io attribute
    unsigned int x_io = getIntValue(reader, "x_io", &ok);
#if XML_PARSER_DEBUG
        printf("  x_io      = %d\n", x_io);
#endif
    if ( ok && (x_io < x_size) ) 
    {
        header->x_io = x_io;
    }
    else 
    {
        printf("[XML ERROR] illegal or missing <x_io> attribute in header\n");
        exit(1);
    }

    /////////// get y_io attribute
    unsigned int y_io = getIntValue(reader, "y_io", &ok);
#if XML_PARSER_DEBUG
        printf("  y_io      = %d\n", y_io);
#endif
    if ( ok &&(y_io < y_size) ) 
    {
        header->y_io = y_io;
    }
    else 
    {
        printf("[XML ERROR] illegal or missing <y_io> attribute in header\n");
        exit(1);
    }

    // check the number of cluster
    if ( (x_size * y_size) >= MAX_CLUSTERS )
    {
        printf("[XML ERROR] Number of clusters cannot be larger than %d\n", MAX_CLUSTERS);
        exit(1);
    }

    ///////// get irq_per_proc attribute
    value = getIntValue(reader, "irq_per_proc", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
        printf("  irq_per_proc = %d\n", value);
#endif
        header->irq_per_proc = value;
    }
    else 
    {
        printf("[XML ERROR] illegal or missing <irq_per_proc> attribute in mapping\n");
        exit(1);
    }

    ///////// get use_ram_disk attribute (default = 0)
    value = getIntValue(reader, "use_ram_disk", &ok);
    if (ok) 
    {
#if XML_PARSER_DEBUG
        printf("  use_ram_disk = %d\n", value);
#endif
        header->use_ram_disk = value;
    }
    else 
    {
        header->use_ram_disk = 0;
    }

    ///////// set other header fields 
    header->globals   = 0;
    header->vspaces   = 0;
    header->psegs     = 0;
    header->vsegs     = 0;
    header->tasks     = 0;
    header->procs     = 0;
    header->irqs      = 0;
    header->periphs   = 0;

    int status = xmlTextReaderRead(reader);
    while (status == 1) 
    {
        const char * tag = (const char *) xmlTextReaderConstName(reader);

        if      (strcmp(tag, "clusterset")   == 0) { clusterSetNode(reader); }
        else if (strcmp(tag, "globalset")    == 0) { globalSetNode(reader); }
        else if (strcmp(tag, "vspaceset")    == 0) { vspaceSetNode(reader); }
        else if (strcmp(tag, "#text")        == 0) { }
        else if (strcmp(tag, "#comment")     == 0) { }
        else if (strcmp(tag, "mapping_info") == 0) 
        {
#if XML_PARSER_DEBUG
            printf("end mapping_info\n");
#endif
            return;
        }
        else 
        {
            printf("[XML ERROR] Unknown tag in header node : %s\n",tag);
            exit(1);
        }
        status = xmlTextReaderRead(reader);
    }
} // end headerNode()


////////////////////////////////////
void BuildTable( int          fdout, 
                 const char * type, 
                 unsigned int nb_elem,
                 unsigned int elem_size, 
                 char ** table ) 
{
    unsigned int i;
    for (i = 0; i < nb_elem; i++) 
    {
        if (elem_size != write(fdout, table[i], elem_size)) 
        {
            printf("function %s: %s(%d) write  error \n", __FUNCTION__, type, i);
            exit(1);
        }

#if XML_PARSER_DEBUG
        printf("Building binary: writing %s %d\n", type, i);
#endif
    }
}

/////////////////////////////////////
int open_file(const char * file_path) 
{
    //open file
    int fdout = open( file_path, (O_CREAT | O_RDWR), (S_IWUSR | S_IRUSR) );
    if (fdout < 0) 
    {
        perror("open");
        exit(1);
    }

    //reinitialise the file
    if (ftruncate(fdout, 0)) 
    {
        perror("truncate");
        exit(1);
    }

    //#if XML_PARSER_DEBUG
    printf("%s\n", file_path);
    //#endif

    return fdout;
}


/////////////////////////////////////
void buildBin(const char * file_path) 
{
    unsigned int length;

    int fdout = open_file(file_path);

#if XML_PARSER_DEBUG
printf("Building map.bin for %s\n", header->name);
printf("signature = %x\n", header->signature);
printf("x_size    = %d\n", header->x_size);
printf("y_size    = %d\n", header->y_size);
printf("x_width   = %d\n", header->x_width);
printf("y_width   = %d\n", header->y_width);
printf("vspaces   = %d\n", header->vspaces);
printf("psegs     = %d\n", header->psegs);
printf("vsegs     = %d\n", header->vsegs);
printf("tasks     = %d\n", header->tasks);
printf("procs     = %d\n", header->procs);
printf("irqs      = %d\n", header->irqs);
printf("periphs   = %d\n", header->periphs);
#endif

    // write header to binary file
    length = write(fdout, (char *) header, sizeof(mapping_header_t));
    if (length != sizeof(mapping_header_t)) 
    {
        printf("write header error : length = %d \n", length);
        exit(1);
    }

    // write clusters
    BuildTable(fdout, "cluster", cluster_index, sizeof(mapping_cluster_t), (char **) cluster);
    // write psegs
    BuildTable(fdout, "pseg", pseg_index, sizeof(mapping_pseg_t), (char **) pseg);
    // write vspaces
    BuildTable(fdout, "vspace", vspace_index, sizeof(mapping_vspace_t), (char **) vspace);
    // write vsegs
    BuildTable(fdout, "vseg", vseg_index, sizeof(mapping_vseg_t), (char **) vseg);
    // write tasks array
    BuildTable(fdout, "task", task_index, sizeof(mapping_task_t), (char **) task);
    //building procs array
    BuildTable(fdout, "proc", proc_index, sizeof(mapping_proc_t), (char **) proc);
    //building irqs array
    BuildTable(fdout, "irq", irq_index, sizeof(mapping_irq_t), (char **) irq);
    //building periphs array
    BuildTable(fdout, "periph", periph_index, sizeof(mapping_periph_t), (char **) periph);

    close(fdout);

} // end buildBin()


//////////////////////////////////////////////////////
char * buildPath(const char * path, const char * name) 
{
    char * res = calloc(strlen(path) + strlen(name) + 1, 1);
    strcat(res, path);
    strcat(res, "/");
    strcat(res, name);
    return res; 
}


//////////////////////////////////
int main(int argc, char * argv[]) 
{
    if (argc < 3) 
    {
        printf("Usage: xml2bin <input_file_path> <output_path>\n");
        return 1;
    }

    struct stat dir_st;
    if (stat( argv[2], &dir_st)) 
    {
        perror("bad path");
        exit(1);
    }

    if ((dir_st.st_mode & S_IFDIR) == 0) 
    {
        printf("path is not a dir: %s", argv[2] );
        exit(1);
    }

    char * map_path = buildPath(argv[2], "map.bin"); 

    LIBXML_TEST_VERSION;

    int status;
    xmlTextReaderPtr reader = xmlReaderForFile(argv[1], NULL, 0);

    if (reader != NULL) 
    {
        status = xmlTextReaderRead (reader);
        while (status == 1) 
        {
            const char * tag = (const char *) xmlTextReaderConstName(reader);

            if (strcmp(tag, "mapping_info") == 0) 
            { 
                headerNode(reader);
                buildBin(map_path);
            }
            else 
            {
                printf("[XML ERROR] Wrong file type: \"%s\"\n", argv[1]);
                return 1;
            }
            status = xmlTextReaderRead(reader);
        }
        xmlFreeTextReader(reader);

        if (status != 0) 
        {
            printf("[XML ERROR] Wrong Syntax in \"%s\" file\n", argv[1]);
            return 1;
        }
    }
    return 0;
} // end main()


// Local Variables:
// tab-width: 4
// c-basic-offset: 4
// c-file-offsets:((innamespace . 0)(inline-open . 0))
// indent-tabs-mode: nil
// End:
// vim: filetype=c:expandtab:shiftwidth=4:tabstop=4:softtabstop=4

