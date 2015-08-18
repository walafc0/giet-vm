////////////////////////////////////////////////////////////////////////////
// File     : xml_driver.c
// Date     : 04/04/2012
// Author   : alain greiner
// Copyright (c) UPMC-LIP6
////////////////////////////////////////////////////////////////////////////
// This program translate a binary file containing a MAPPING_INFO 
// data structure to an xml file.
////////////////////////////////////////////////////////////////////////////

#include  <stdlib.h>
#include  <fcntl.h>
#include  <unistd.h>
#include  <stdio.h>
#include  <string.h>
#include  <stdint.h>
#include  <mapping_info.h>

//////////////////////////////////////////////////////
void buildXml(mapping_header_t * header, FILE * fpout) 
{
    // mnemonics defined in mapping_info.h
    const char * vseg_type[] = 
    { 
        "ELF",        // From a .elf file
        "BLOB",       // Raw Binary 
        "PTAB",       // Page Table 
        "PERI",       // Hardware Component
        "BUFFER",     // No intialisation needed (stacks...)
        "SCHED",      // Scheduler
        "HEAP",       // Heap     
    };

    // mnemonics defined in mapping_info.h
    const char * pseg_type[] = 
    {
        "RAM",
        "PERI",
    };

    // mnemonics defined in mapping_info.h
    const char * irq_type[] =
    {
        "HWI",                      
        "WTI",
        "PTI",
    };

    // These mnemonics must be consistent with values in
    // irq_handler.h / irq_handler.c / mapping.py
    const char * isr_type[] =
    {
        "ISR_DEFAULT", 
        "ISR_TICK",
        "ISR_TTY_RX",
        "ISR_TTY_TX",
        "ISR_BDV",
        "ISR_TIMER",
        "ISR_WAKUP",
        "ISR_NIC_RX",
        "ISR_NIC_TX",
        "ISR_CMA",
        "ISR_MMC",
        "ISR_DMA",
        "ISR_SDC",
        "ISR_MWR",
        "ISR_HBA",
    };

    const char * mwr_subtype[] =
    {
        "GCD",
        "DCT",
        "CPY",
    };

    const char * periph_type[] =
    {
        "CMA",
        "DMA",
        "FBF",
        "IOB",
        "IOC",
        "MMC",
        "MWR",
        "NIC",
        "ROM",
        "SIM",
        "TIM",
        "TTY",
        "XCU",
        "PIC",
        "DROM",
    };

    const char * ioc_subtype[] =
    {
        "BDV",
        "HBA",
        "SDC",
        "SPI",
    };

    const char * mode_str[] = 
    { 
        "____",
        "___U", 
        "__W_", 
        "__WU", 
        "_X__", 
        "_X_U", 
        "_XW_", 
        "_XWU", 
        "C___", 
        "C__U", 
        "C_W_", 
        "C_WU", 
        "CX__", 
        "CX_U", 
        "CXW_", 
        "CXWU", 
    };

    unsigned int vspace_id;
    unsigned int cluster_id;
    unsigned int pseg_id;
    unsigned int vseg_id;
    unsigned int task_id;
    unsigned int proc_id;
    unsigned int irq_id;
    unsigned int periph_id;

    mapping_cluster_t * cluster;
    mapping_pseg_t * pseg;
    mapping_vspace_t * vspace;
    mapping_vseg_t * vseg;
    mapping_task_t * task;
    mapping_irq_t * irq;    
    mapping_periph_t * periph;

    // computes the base adresss for clusters array, 
    cluster = (mapping_cluster_t *)((char *) header +
            MAPPING_HEADER_SIZE);

    // computes the base adresss for psegs array, 
    pseg = (mapping_pseg_t *) ((char *) header +
            MAPPING_HEADER_SIZE +
            MAPPING_CLUSTER_SIZE * header->x_size * header->y_size);

    // computes the base adresss for vspaces array, 
    vspace = (mapping_vspace_t *) ((char *) header +
            MAPPING_HEADER_SIZE +
            MAPPING_CLUSTER_SIZE * header->x_size * header->y_size +
            MAPPING_PSEG_SIZE * header->psegs);

    // computes the base adresss for vsegs array, 
    vseg = (mapping_vseg_t *) ((char *) header +
            MAPPING_HEADER_SIZE +
            MAPPING_CLUSTER_SIZE * header->x_size * header->y_size +
            MAPPING_PSEG_SIZE * header->psegs +
            MAPPING_VSPACE_SIZE * header->vspaces);

    // computes the base address for tasks array 
    task = (mapping_task_t *) ((char *) header +
            MAPPING_HEADER_SIZE +
            MAPPING_CLUSTER_SIZE * header->x_size * header->y_size +
            MAPPING_PSEG_SIZE * header->psegs +
            MAPPING_VSPACE_SIZE * header->vspaces +
            MAPPING_VSEG_SIZE * header->vsegs);

    // computes the base address for irqs array 
    irq = (mapping_irq_t *) ((char *) header +
            MAPPING_HEADER_SIZE +
            MAPPING_CLUSTER_SIZE * header->x_size * header->y_size +
            MAPPING_PSEG_SIZE * header->psegs +
            MAPPING_VSPACE_SIZE * header->vspaces +
            MAPPING_VSEG_SIZE * header->vsegs +
            MAPPING_TASK_SIZE * header->tasks +
            MAPPING_PROC_SIZE * header->procs);

    // computes the base address for periphs array 
    periph = (mapping_periph_t *) ((char *) header +
            MAPPING_HEADER_SIZE +
            MAPPING_CLUSTER_SIZE * header->x_size * header->y_size +
            MAPPING_PSEG_SIZE * header->psegs +
            MAPPING_VSPACE_SIZE * header->vspaces +
            MAPPING_VSEG_SIZE * header->vsegs +
            MAPPING_TASK_SIZE * header->tasks +
            MAPPING_PROC_SIZE * header->procs +
            MAPPING_IRQ_SIZE * header->irqs);

    ///////////////////////// header /////////////////////////////////////////////

    fprintf(fpout, "<?xml version = \"1.0\"?>\n\n");

    fprintf(fpout, "<mapping_info signature    = \"0x%x\" \n" , header->signature);
    fprintf(fpout, "              name         = \"%s\"   \n" , header->name);
    fprintf(fpout, "              x_size       = \"%d\"   \n" , header->x_size);
    fprintf(fpout, "              y_size       = \"%d\"   \n" , header->y_size);
    fprintf(fpout, "              x_width      = \"%d\"   \n" , header->x_width);
    fprintf(fpout, "              y_width      = \"%d\"   \n" , header->y_width);
    fprintf(fpout, "              irq_per_proc = \"%d\"   \n" , header->irq_per_proc);
    fprintf(fpout, "              use_ram_disk = \"%d\"   \n" , header->use_ram_disk);
    fprintf(fpout, "              x_io         = \"%d\"   \n" , header->x_io);
    fprintf(fpout, "              y_io         = \"%d\" >\n\n", header->y_io);

    ///////////////////// clusters ///////////////////////////////////////////////

    fprintf( fpout, "    <clusterset>\n");
    for (cluster_id = 0; cluster_id < (header->x_size * header->y_size); cluster_id++) 
    {
        fprintf(fpout, "        <cluster x=\"%d\" y=\"%d\" >\n", 
                cluster[cluster_id].x, cluster[cluster_id].y );

        ///////////////////// psegs   ////////////////////////////////////////////////

        for (pseg_id = cluster[cluster_id].pseg_offset;
             pseg_id < cluster[cluster_id].pseg_offset + cluster[cluster_id].psegs;
             pseg_id++) 
        {
            fprintf(fpout, "            <pseg name=\"%s\"", pseg[pseg_id].name);
            fprintf(fpout, " type=\"%s\"", pseg_type[pseg[pseg_id].type]);
            fprintf(fpout, " base=\"0x%llx\"", pseg[pseg_id].base);
            fprintf(fpout, " length=\"0x%llx\" />\n", pseg[pseg_id].length);
        }

        ///////////////////// processors /////////////////////////////////////////////

        unsigned int proc_index = 0;
        for (proc_id = cluster[cluster_id].proc_offset;
             proc_id < cluster[cluster_id].proc_offset + cluster[cluster_id].procs;
             proc_id++, proc_index++) 
        {
            fprintf(fpout, "            <proc index=\"%d\" />\n", proc_index);
        }

        ///////////////////// periphs  ///////////////////////////////////////////////

        for (periph_id = cluster[cluster_id].periph_offset;
             periph_id < cluster[cluster_id].periph_offset + cluster[cluster_id].periphs;
             periph_id++) 
        {
            fprintf(fpout, "            <periph type=\"%s\"", periph_type[periph[periph_id].type]);

            if (periph[periph_id].type == PERIPH_TYPE_IOC) 
                fprintf(fpout, " subtype=\"%s\"", ioc_subtype[periph[periph_id].subtype]);

            if (periph[periph_id].type == PERIPH_TYPE_MWR) 
                fprintf(fpout, " subtype=\"%s\"", mwr_subtype[periph[periph_id].subtype]);

            fprintf(fpout, " psegname=\"%s\"", pseg[periph[periph_id].psegid].name);
            fprintf(fpout, " channels=\"%d\"",  periph[periph_id].channels);
            fprintf(fpout, " arg0=\"%d\"",  periph[periph_id].arg0);
            fprintf(fpout, " arg1=\"%d\"",  periph[periph_id].arg1);
            fprintf(fpout, " arg2=\"%d\"",  periph[periph_id].arg2);
            fprintf(fpout, " arg3=\"%d\" >\n",  periph[periph_id].arg3);
            if ( (periph[periph_id].type == PERIPH_TYPE_PIC) ||
                 (periph[periph_id].type == PERIPH_TYPE_XCU) )
            {
                for (irq_id = periph[periph_id].irq_offset; 
                     irq_id < periph[periph_id].irq_offset + periph[periph_id].irqs;
                     irq_id++) 
                {
                    fprintf(fpout, "                <irq srctype=\"%s\"", irq_type[irq[irq_id].srctype]);
                    fprintf(fpout, " srcid=\"%d\"", irq[irq_id].srcid);
                    fprintf(fpout, " isr=\"%s\"", isr_type[irq[irq_id].isr]);
                    fprintf(fpout, " channel=\"%d\" />\n", irq[irq_id].channel);
                }
            }
            fprintf(fpout, "            </periph>\n");
        }
        fprintf(fpout, "        </cluster>\n" );
    }
    fprintf(fpout, "    </clusterset>\n\n" );

    /////////////////// globals /////////////////////////////////////////////////

    fprintf(fpout, "    <globalset>\n" );
    for (vseg_id = 0; vseg_id < header->globals; vseg_id++) 
    {
        unsigned int pseg_id    = vseg[vseg_id].psegid; 
        unsigned int cluster_id = pseg[pseg_id].clusterid;

        fprintf(fpout, "        <vseg name=\"%s\"", vseg[vseg_id].name);
        fprintf(fpout, " vbase=\"0x%x\"", vseg[vseg_id].vbase);
        fprintf(fpout, " length=\"0x%x\"", vseg[vseg_id].length);
        fprintf(fpout, " type=\"%s\"", vseg_type[vseg[vseg_id].type]);
        fprintf(fpout, " mode=\"%s\"", mode_str[vseg[vseg_id].mode]);
        fprintf(fpout, "\n             ");      
        fprintf(fpout, " x=\"%d\"", cluster[cluster_id].x);
        fprintf(fpout, " y=\"%d\"", cluster[cluster_id].y);
        fprintf(fpout, " psegname=\"%s\"", pseg[pseg_id].name);
        if( vseg[vseg_id].ident ) 
        fprintf(fpout, " ident=\"1\"");
        if( vseg[vseg_id].local ) 
        fprintf(fpout, " local=\"1\"");
        if( vseg[vseg_id].big ) 
        fprintf(fpout, " big=\"1\"");
        if( vseg[vseg_id].binpath[0] != 0 ) 
        fprintf(fpout, " binpath=\"%s\"", vseg[vseg_id].binpath);
        fprintf(fpout, " >\n");
    }
    fprintf(fpout, "    </globalset>\n" );

    //////////////////// vspaces ////////////////////////////////////////////////

    fprintf( fpout, "\n    <vspaceset>\n\n" );
    for (vspace_id = 0; vspace_id < header->vspaces; vspace_id++) 
    {
        unsigned int vseg_id = vspace[vspace_id].start_vseg_id;
        fprintf(fpout, "        <vspace name=\"%s\"", vspace[vspace_id].name); 
        fprintf(fpout, " startname=\"%s\"", vseg[vseg_id].name); 
        fprintf(fpout, " active=\"%d\" >\n", vspace[vspace_id].active); 

        //////////////////// vsegs //////////////////////////////////////////////

        for (vseg_id = vspace[vspace_id].vseg_offset;
             vseg_id < (vspace[vspace_id].vseg_offset + vspace[vspace_id].vsegs);
             vseg_id++) 
        {
            unsigned int pseg_id    = vseg[vseg_id].psegid; 
            unsigned int cluster_id = pseg[pseg_id].clusterid;

            fprintf(fpout, "            <vseg name=\"%s\"", vseg[vseg_id].name);
            fprintf(fpout, " vbase=\"0x%x\"", vseg[vseg_id].vbase);
            fprintf(fpout, " length=\"0x%x\"", vseg[vseg_id].length);
            fprintf(fpout, " type=\"%s\"", vseg_type[vseg[vseg_id].type]);
            fprintf(fpout, " mode=\"%s\"", mode_str[vseg[vseg_id].mode]);
            fprintf(fpout, "\n                 ");      
            fprintf(fpout, " x=\"%d\"", cluster[cluster_id].x);
            fprintf(fpout, " y=\"%d\"", cluster[cluster_id].y);
            fprintf(fpout, " psegname=\"%s\"", pseg[pseg_id].name);
            if( vseg[vseg_id].ident ) 
            fprintf(fpout, " ident=\"1\"");
            if( vseg[vseg_id].local ) 
            fprintf(fpout, " local=\"1\"");
            if( vseg[vseg_id].big ) 
            fprintf(fpout, " big=\"1\"");
            if( vseg[vseg_id].binpath[0] != 0 ) 
            fprintf(fpout, " binpath=\"%s\"", vseg[vseg_id].binpath);
            fprintf(fpout, " >\n");
        }

        //////////////////// tasks //////////////////////////////////////////////

        for (task_id = vspace[vspace_id].task_offset;
             task_id < (vspace[vspace_id].task_offset + vspace[vspace_id].tasks);
             task_id++) 
        {
            unsigned int stack_vseg_id = task[task_id].stack_vseg_id; 
            unsigned int heap_vseg_id  = task[task_id].heap_vseg_id; 
            unsigned int cluster_id    = task[task_id].clusterid;

            fprintf(fpout, "            <task name=\"%s\"", task[task_id].name);
            fprintf(fpout, " trdid=\"%d\"", task[task_id].trdid);
            fprintf(fpout, " x=\"%d\"", cluster[cluster_id].x);
            fprintf(fpout, " y=\"%d\"", cluster[cluster_id].y);
            fprintf(fpout, " p=\"%d\"", task[task_id].proclocid);
            fprintf(fpout, "\n                 ");      
            fprintf(fpout, " stackname=\"%s\"", vseg[stack_vseg_id].name);
            if (heap_vseg_id != -1) 
            fprintf(fpout, " heapname=\"%s\"", vseg[heap_vseg_id].name);
            fprintf(fpout, " startid = \"%d\"", task[task_id].startid);
            fprintf(fpout, " />\n");
        }
        fprintf(fpout, "        </vspace>\n\n");
    }
    fprintf(fpout, "    </vspaceset>\n");
    fprintf(fpout, "</mapping_info>\n");
} // end buildXml()


/////////////////////////////////////
int main(int argc, char * argv[]) 
{
    if (argc < 2) 
    {
        printf("Usage: bin2xml <input_file_path> <output_file_path>\n");
        return 1;
    }

    unsigned int bin[0x10000]; // 64 K int = 256 Kbytes 

    int fdin = open(argv[1], O_RDONLY);
    if (fdin < 0) 
    {
        perror("open");
        exit(1);
    }

    FILE * fpout = fopen( argv[2], "w");
    if (fpout == NULL) 
    {
        perror("open");
        exit(1);
    }

    unsigned int length = read(fdin, bin, 0x40000);

    if (length <= 0) 
    {
        perror("read");
        exit(1);
    }

    if (bin[0] == IN_MAPPING_SIGNATURE) 
    {
        buildXml((mapping_header_t *) bin, fpout);
    } 
    else 
    {
        printf("[ERROR] Wrong file format\n");
        exit(1);
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

