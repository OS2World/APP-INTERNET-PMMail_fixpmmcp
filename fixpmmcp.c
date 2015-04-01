#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Out of frustation for PMMail/2 not using 8 bit transfer encoding and
   charset iso-8859-1 codepage by default, and not recognizing "windows"
   charset, I wrote yet another command line patch ... use this as your
   filter for every mail, I guess...  Samuel Audet <guardia@cam.org> */

typedef struct
{
   char boundary[256];
   char codepage[256];
   char content_type[256];
   char transfer_encoding[256];
   char mime_version[16];
} HEADER_INFO;

HEADER_INFO main_header;

char *blank_strip(char *string)
{
   int i = 0;
   char *pos = string;
   while (*pos == ' ' || *pos == '\t' ||
          *pos == '\n' || *pos == '\r') pos++;
   i = strlen(pos)+1;

   if(pos != string)
      memmove(string,pos,i);

   i-=2;
   while (string[i] == ' ' || string[i] == '\t' ||
          string[i] == '\n' || string[i] == '\r') { string[i] = 0; i--; }

   return string;
}

char *remove_quotes(char *string)
{
   if(string[0] == '"')
      memmove(string,string+1,strlen(string)+1);
   if(string[strlen(string)-1] == '"')
      string[strlen(string)-1] = 0;

   return string;
}

int char_pos(char *string, char car)
{
   char *pos = strchr(string,car);
   if(pos == NULL)
      return -1;
   else
      return pos-string;
}

#define split_field(field) char_pos(field,':')
#define split_value(value) char_pos(value,'=')

void read_header(FILE *file, HEADER_INFO *header_info)
{
   char line[256];

   header_info->boundary[0] = 0;
   header_info->codepage[0] = 0;
   header_info->content_type[0] = 0;
   header_info->transfer_encoding[0] = 0;

   while(fgets(line, sizeof(line), file) != NULL)
   {
      int split_pos;

      blank_strip(line);

      if(line[0] == 0)
         break;

      split_pos = split_field(line);

      if(split_pos == -1)
      {
         ;
      }
      else if(strnicmp(line,"Content-Type",split_pos) == 0)
      {
         char values[256];
         char line2[256];
         char *value;

         strcpy(values, line+split_pos+1);
         blank_strip(values);

         while(values[strlen(values)-1] == ';' &&
               fgets(line2, sizeof(line2), file) != NULL)
         {
            blank_strip(line2);
            strncat(values,line2,sizeof(values)-strlen(values)-1);

            if(line2[0] == 0)
               break;
         }

         value = strtok(values,";");
         while(value != NULL)
         {
            blank_strip(value);
            split_pos = split_value(value);
            if(split_pos == -1) // we have the actual content-type
            {
               strncpy(header_info->content_type,value,sizeof(header_info->content_type));
               blank_strip(header_info->content_type);
               remove_quotes(header_info->content_type);
            }                             
            else if(strnicmp(value,"charset",split_pos) == 0)
            {
               strncpy(header_info->codepage,value+split_pos+1,sizeof(header_info->codepage));
               blank_strip(header_info->codepage);
               remove_quotes(header_info->codepage);
            }
            else if(strnicmp(value,"boundary",split_pos) == 0)
            {
               strncpy(header_info->boundary,value+split_pos+1,sizeof(header_info->boundary));
               blank_strip(header_info->boundary);
               remove_quotes(header_info->boundary);
            }

            value = strtok(NULL,";");
         }

      }
      else if(strnicmp(line,"Content-Transfer-Encoding",split_pos) == 0)
      {
         strncpy(header_info->transfer_encoding,line+split_pos+1,sizeof(header_info->transfer_encoding));
         blank_strip(header_info->transfer_encoding);
         remove_quotes(header_info->transfer_encoding);
      }
      else if(strnicmp(line,"MIME-Version",split_pos) == 0)
      {
         strncpy(header_info->mime_version,line+split_pos+1,sizeof(header_info->mime_version));
         blank_strip(header_info->mime_version);
         remove_quotes(header_info->mime_version);
      }

   }

}

void copyfix_header(FILE *file_read, FILE *file_write, HEADER_INFO *header, int is_main_header)
{
   char line[256];

   // fix crap that PMMail/2 doesn't like
   strlwr(header->codepage);
   if(header->codepage[0] == 0 ||
      strstr(header->codepage,"windows") != NULL ||
      strstr(header->codepage,"ascii") != NULL)
   {
      if(header->content_type[0] == 0) // no content-type?? use text/plain
         strcpy(header->content_type,"text/plain");
      fprintf(file_write, "Content-Type: %s; charset=iso-8859-1", header->content_type);
      if(header->boundary[0] != 0)
         fprintf(file_write, ";boundary=\"%s\"", header->boundary);
      fprintf(file_write,"\n");
   }
   if(header->transfer_encoding[0] == 0)
   {
      fprintf(file_write, "Content-Transfer-Encoding: 8bit\n");
   }
   if(is_main_header && header->mime_version[0] == 0)
   {
      fprintf(file_write, "MIME-Version: 1.0\n");
   }

   while(fgets(line, sizeof(line), file_read) != NULL)
   {
      if(line[0] == '\n') // is this enough to detect end of headers?
         break;

      fputs(line, file_write);
   }

   fputc('\n',file_write);
}

void process_parts(FILE *file_read, FILE *file_write, char *boundary)
{
   char line[256];
   HEADER_INFO aheader;

   if(boundary[0] == 0) // no parts
   {
      while(fgets(line, sizeof(line), file_read) != NULL)
         fputs(line, file_write);
   }
   else
   {
      while(fgets(line, sizeof(line), file_read) != NULL)
      {
         // if we hit the beginning of a part, fix the header
         if(line[0] == '-' && line[1] == '-' &&
            strncmp(line+2,boundary,strlen(line)-3) == 0)
         {
            int file_pos = ftell(file_read);

            fputs(line, file_write);

            read_header(file_read,&aheader);
            fseek(file_read,file_pos,SEEK_SET);
            copyfix_header(file_read,file_write,&aheader,0);
         }
         else
            fputs(line, file_write);

      }
   }

}

int main(int argc, char *argv[])
{
   FILE *file_read, *file_write;
   char *filename = argv[1];
   char backup_filename[256];
   char *dotpos;

   dotpos = strrchr(filename,'.');
   if(dotpos == NULL)
      strcpy(backup_filename,filename);
   else
   {
      strncpy(backup_filename,filename,dotpos-filename);
      backup_filename[dotpos-filename] = 0;
   }

   strcat(backup_filename,".bak");

   if(rename(filename,backup_filename) != 0)
   {
      perror(backup_filename);
      perror(filename);
      return 1;
   }

   file_read = fopen(backup_filename,"r");
   if(file_read == NULL)
   {
      perror(backup_filename);
      return 2;
   }

   file_write = fopen(filename,"w");
   if(file_write == NULL)
   {
      perror(filename);
      return 3;
   }

   read_header(file_read,&main_header);
   rewind(file_read);
   copyfix_header(file_read,file_write,&main_header,1);

   process_parts(file_read,file_write,main_header.boundary);

   fclose(file_read);
   fclose(file_write);
   remove(backup_filename);

   return 0;
}
